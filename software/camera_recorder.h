/*
 * camera_recorder.h — RKMPI CSI camera recording API
 *                     for Luckfox Pico Mini B (RV1103)
 *
 * Pipeline:  VI (ISP) ──bind──> VENC (H.264) ──thread──> .h264 file
 *
 * Prerequisites (on the board before running):
 *   RkLunch-stop.sh          # release camera from default rkipc daemon
 *
 * Cross-compile with the Luckfox SDK toolchain and link:
 *   -lrockit -lrockiva -lpthread
 *
 * Usage:
 *   RecorderConfig cfg = RECORDER_CONFIG_DEFAULT;
 *   cfg.output_path = "/tmp/clip.h264";
 *   recorder_start(&cfg);
 *   sleep(10);
 *   recorder_stop();
 */

#ifndef CAMERA_RECORDER_H
#define CAMERA_RECORDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Public types ────────────────────────────────────────────────── */

typedef enum {
    REC_CODEC_H264 = 0,
    REC_CODEC_H265,
} RecorderCodec;

typedef struct {
    /* Video dimensions */
    uint32_t      width;         /* default 1920 */
    uint32_t      height;        /* default 1080 */
    uint32_t      fps;           /* default 30   */

    /* Encoder settings */
    RecorderCodec codec;         /* default H.264 */
    uint32_t      bitrate_kbps;  /* default 4096 kbps */

    /* VI pipe/channel IDs (usually 0 for Pico Mini B) */
    int           vi_pipe;       /* default 0 */
    int           vi_chn;        /* default 0 */
    int           venc_chn;      /* default 0 */

    /* Output file path — must be writable */
    const char   *output_path;   /* default "/tmp/recording.h264" */
} RecorderConfig;

/* Sensible defaults — override only what you need */
#define RECORDER_CONFIG_DEFAULT {   \
    .width        = 1920,          \
    .height       = 1080,          \
    .fps          = 30,            \
    .codec        = REC_CODEC_H264,\
    .bitrate_kbps = 4096,          \
    .vi_pipe      = 0,             \
    .vi_chn       = 0,             \
    .venc_chn     = 0,             \
    .output_path  = "/mnt/sdcard/DCIM/Development/recording.h264", \
}

/* ── Public API ──────────────────────────────────────────────────── */

/**
 * recorder_start() — Initialise the MPI pipeline and begin recording.
 *
 * Spawns an internal writer thread that drains the VENC stream to disk.
 * Returns 0 on success, -1 on any MPI or file-system error.
 * Calling start() while already recording is a no-op (returns 0).
 */
int recorder_start(const RecorderConfig *cfg);

/**
 * recorder_stop() — Flush pending frames, close the output file,
 *                   and tear down the MPI pipeline.
 *
 * Blocks until the writer thread has exited cleanly.
 * Safe to call even if recorder_start() was never called.
 */
void recorder_stop(void);

/**
 * recorder_is_running() — Returns 1 if recording is active, 0 otherwise.
 */
int recorder_is_running(void);

#ifdef __cplusplus
}
#endif
#endif /* CAMERA_RECORDER_H */
