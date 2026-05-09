/*
 * camera_recorder.c — RKMPI CSI camera recording implementation
 *                     for Luckfox Pico Mini B (RV1103)
 *
 * Pipeline:
 *   VI (ISP, NV12) ──RK_MPI_SYS_Bind──> VENC (H.264/H.265)
 *                                              │
 *                                     writer thread (this file)
 *                                              │
 *                                        output .h264 file
 *
 * The writer thread calls RK_MPI_VENC_GetStream() in a tight loop,
 * writes each NAL unit to the output file, releases the stream buffer,
 * and exits cleanly when g_running is cleared by recorder_stop().
 */

#include "camera_recorder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

/* ── RKMPI / Rockit headers (provided by the Luckfox SDK) ────────── */
#include "rk_mpi_sys.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_venc.h"
#include "rk_type.h"
#include "rk_comm_video.h"
#include "rk_comm_venc.h"

/* ── Internal state ──────────────────────────────────────────────── */

static atomic_int  g_running   = 0;   /* 1 while recording */
static pthread_t   g_writer_tid;
static FILE       *g_out_file  = NULL;
static RecorderConfig g_cfg;

/* ── Helpers ─────────────────────────────────────────────────────── */

#define LOG_ERR(fmt, ...) \
    fprintf(stderr, "[recorder] ERROR: " fmt "\n", ##__VA_ARGS__)

#define LOG_INF(fmt, ...) \
    fprintf(stdout, "[recorder] " fmt "\n", ##__VA_ARGS__)

#define CHECK_RET(ret, msg) do {                    \
    if ((ret) != RK_SUCCESS) {                      \
        LOG_ERR(msg " (ret=0x%08x)", (unsigned)(ret)); \
        return -1;                                  \
    }                                               \
} while (0)

/* ── Writer thread ───────────────────────────────────────────────── */

/*
 * Each iteration of the loop dequeues one encoded stream packet from
 * the VENC channel, writes it to disk, then releases the buffer back
 * to the codec.  RK_MPI_VENC_GetStream() blocks up to `timeout_ms`
 * milliseconds, so the thread is never busy-spinning.
 */
static void *writer_thread(void *arg)
{
    (void)arg;
    const int timeout_ms = 500;
    VENC_STREAM_S stream;
    memset(&stream, 0, sizeof(stream));

    LOG_INF("writer thread started (chn=%d)", g_cfg.venc_chn);

    while (atomic_load(&g_running)) {
        RK_S32 ret = RK_MPI_VENC_GetStream(g_cfg.venc_chn, &stream, timeout_ms);
        if (ret == RK_ERR_VENC_BUF_EMPTY) {
            /* No frame ready yet — loop and try again */
            continue;
        }
        if (ret != RK_SUCCESS) {
            LOG_ERR("RK_MPI_VENC_GetStream failed (0x%08x)", (unsigned)ret);
            break;
        }

        /* Write all NAL units in this stream packet to the output file */
        for (RK_U32 i = 0; i < stream.u32PackCount; i++) {
            VENC_PACK_S *pack = &stream.pstPack[i];
            if (pack->u32Len > 0 && pack->pData != NULL) {
                fwrite(pack->pData, 1, pack->u32Len, g_out_file);
            }
        }

        /* Release the stream buffer back to the codec — MANDATORY */
        RK_MPI_VENC_ReleaseStream(g_cfg.venc_chn, &stream);
    }

    LOG_INF("writer thread exiting");
    return NULL;
}

/* ── VI (Video Input) setup ──────────────────────────────────────── */

static int setup_vi(const RecorderConfig *cfg)
{
    RK_S32 ret;

    /* Enable VI device (corresponds to the ISP/sensor) */
    ret = RK_MPI_VI_EnableDev(cfg->vi_pipe);
    CHECK_RET(ret, "RK_MPI_VI_EnableDev");

    /* Configure VI channel attributes */
    VI_CHN_ATTR_S vi_attr;
    memset(&vi_attr, 0, sizeof(vi_attr));
    vi_attr.stIspOpt.u32BufCount     = 3;            /* triple-buffer */
    vi_attr.stIspOpt.enMemoryType    = VI_V4L2_MEMORY_TYPE_MMAP;
    vi_attr.stSize.u32Width           = cfg->width;
    vi_attr.stSize.u32Height          = cfg->height;
    vi_attr.enPixelFormat             = RK_FMT_YUV420SP; /* NV12 */
    vi_attr.enCompressMode            = COMPRESS_MODE_NONE;
    vi_attr.f32FrameRate              = (float)cfg->fps;

    ret = RK_MPI_VI_SetChnAttr(cfg->vi_pipe, cfg->vi_chn, &vi_attr);
    CHECK_RET(ret, "RK_MPI_VI_SetChnAttr");

    ret = RK_MPI_VI_EnableChn(cfg->vi_pipe, cfg->vi_chn);
    CHECK_RET(ret, "RK_MPI_VI_EnableChn");

    LOG_INF("VI ready: %ux%u @%ufps (pipe=%d chn=%d)",
            cfg->width, cfg->height, cfg->fps,
            cfg->vi_pipe, cfg->vi_chn);
    return 0;
}

/* ── VENC (Video Encoder) setup ──────────────────────────────────── */

static int setup_venc(const RecorderConfig *cfg)
{
    RK_S32 ret;

    VENC_CHN_ATTR_S venc_attr;
    memset(&venc_attr, 0, sizeof(venc_attr));

    /* Select codec type */
    if (cfg->codec == REC_CODEC_H265) {
        venc_attr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
        venc_attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
        venc_attr.stRcAttr.stH265Cbr.u32Gop         = cfg->fps * 2;
        venc_attr.stRcAttr.stH265Cbr.u32BitRate      = cfg->bitrate_kbps * 1000;
        venc_attr.stRcAttr.stH265Cbr.fr32DstFrameRate = (float)cfg->fps;
        venc_attr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
    } else {
        /* Default: H.264 */
        venc_attr.stVencAttr.enType = RK_VIDEO_ID_AVC;
        venc_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        venc_attr.stRcAttr.stH264Cbr.u32Gop          = cfg->fps * 2;
        venc_attr.stRcAttr.stH264Cbr.u32BitRate       = cfg->bitrate_kbps * 1000;
        venc_attr.stRcAttr.stH264Cbr.fr32DstFrameRate = (float)cfg->fps;
        venc_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    }

    venc_attr.stVencAttr.u32PicWidth    = cfg->width;
    venc_attr.stVencAttr.u32PicHeight   = cfg->height;
    venc_attr.stVencAttr.u32VirWidth    = cfg->width;
    venc_attr.stVencAttr.u32VirHeight   = cfg->height;
    venc_attr.stVencAttr.u32StreamBufCnt = 4;
    venc_attr.stVencAttr.u32BufSize     = cfg->width * cfg->height * 3 / 2;
    venc_attr.stVencAttr.enPixelFormat  = RK_FMT_YUV420SP;

    ret = RK_MPI_VENC_CreateChn(cfg->venc_chn, &venc_attr);
    CHECK_RET(ret, "RK_MPI_VENC_CreateChn");

    /* Tell the encoder to receive frames continuously */
    VENC_RECV_PIC_PARAM_S recv_param;
    memset(&recv_param, 0, sizeof(recv_param));
    recv_param.s32RecvPicNum = -1;   /* -1 = unlimited */
    ret = RK_MPI_VENC_StartRecv(cfg->venc_chn, &recv_param);
    CHECK_RET(ret, "RK_MPI_VENC_StartRecv");

    LOG_INF("VENC ready: %s %u kbps (chn=%d)",
            cfg->codec == REC_CODEC_H265 ? "H.265" : "H.264",
            cfg->bitrate_kbps,
            cfg->venc_chn);
    return 0;
}

/* ── Bind VI → VENC ──────────────────────────────────────────────── */

static int bind_vi_to_venc(const RecorderConfig *cfg)
{
    MPP_CHN_S src  = { .enModId = RK_ID_VI,   .s32DevId = cfg->vi_pipe, .s32ChnId = cfg->vi_chn  };
    MPP_CHN_S dst  = { .enModId = RK_ID_VENC, .s32DevId = 0,            .s32ChnId = cfg->venc_chn };

    RK_S32 ret = RK_MPI_SYS_Bind(&src, &dst);
    CHECK_RET(ret, "RK_MPI_SYS_Bind(VI->VENC)");

    LOG_INF("VI→VENC bound");
    return 0;
}

static void unbind_vi_from_venc(const RecorderConfig *cfg)
{
    MPP_CHN_S src = { .enModId = RK_ID_VI,   .s32DevId = cfg->vi_pipe, .s32ChnId = cfg->vi_chn  };
    MPP_CHN_S dst = { .enModId = RK_ID_VENC, .s32DevId = 0,            .s32ChnId = cfg->venc_chn };
    RK_MPI_SYS_UnBind(&src, &dst);
}

/* ── Public API ──────────────────────────────────────────────────── */

int recorder_start(const RecorderConfig *cfg)
{
    if (atomic_load(&g_running)) {
        LOG_INF("already recording — ignoring start()");
        return 0;
    }

    /* Copy config so caller does not need to keep it alive */
    g_cfg = *cfg;

    /* Open the output file first — fail fast before touching MPI */
    g_out_file = fopen(cfg->output_path, "wb");
    if (!g_out_file) {
        LOG_ERR("cannot open output file '%s': %s",
                cfg->output_path, strerror(errno));
        return -1;
    }

    /* Initialise MPI system */
    RK_S32 ret = RK_MPI_SYS_Init();
    if (ret != RK_SUCCESS) {
        LOG_ERR("RK_MPI_SYS_Init failed (0x%08x)", (unsigned)ret);
        goto err_file;
    }

    /* Bring up VI, VENC, then wire them together */
    if (setup_vi(cfg)    < 0) goto err_sys;
    if (setup_venc(cfg)  < 0) goto err_vi;
    if (bind_vi_to_venc(cfg) < 0) goto err_venc;

    /* Mark running before spawning the writer so the thread sees it */
    atomic_store(&g_running, 1);

    if (pthread_create(&g_writer_tid, NULL, writer_thread, NULL) != 0) {
        LOG_ERR("pthread_create: %s", strerror(errno));
        atomic_store(&g_running, 0);
        goto err_bind;
    }

    LOG_INF("recording started → %s", cfg->output_path);
    return 0;

    /* ── Teardown on error ── */
err_bind:
    unbind_vi_from_venc(cfg);
err_venc:
    RK_MPI_VENC_StopRecv(cfg->venc_chn);
    RK_MPI_VENC_DestroyChn(cfg->venc_chn);
err_vi:
    RK_MPI_VI_DisableChn(cfg->vi_pipe, cfg->vi_chn);
    RK_MPI_VI_DisableDev(cfg->vi_pipe);
err_sys:
    RK_MPI_SYS_Exit();
err_file:
    fclose(g_out_file);
    g_out_file = NULL;
    return -1;
}

void recorder_stop(void)
{
    if (!atomic_load(&g_running)) {
        LOG_INF("not recording — ignoring stop()");
        return;
    }

    /* Signal the writer thread to finish after the current frame */
    atomic_store(&g_running, 0);

    /* Wait for the writer to flush and exit */
    pthread_join(g_writer_tid, NULL);

    /* Tear down MPI pipeline in reverse order */
    unbind_vi_from_venc(&g_cfg);

    RK_MPI_VENC_StopRecv(g_cfg.venc_chn);
    RK_MPI_VENC_DestroyChn(g_cfg.venc_chn);

    RK_MPI_VI_DisableChn(g_cfg.vi_pipe, g_cfg.vi_chn);
    RK_MPI_VI_DisableDev(g_cfg.vi_pipe);

    RK_MPI_SYS_Exit();

    /* Close the output file */
    if (g_out_file) {
        fflush(g_out_file);
        fclose(g_out_file);
        g_out_file = NULL;
    }

    LOG_INF("recording stopped → %s", g_cfg.output_path);
}

int recorder_is_running(void)
{
    return atomic_load(&g_running);
}
