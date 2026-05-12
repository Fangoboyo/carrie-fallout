/*
 * camera_recorder_demo.c — Demonstrates recorder_start() / recorder_stop()
 *                          on the Luckfox Pico Mini B (RV1103)
 *
 * Board setup (run once on the board before launching this program):
 *   RkLunch-stop.sh          ← releases camera from the default rkipc daemon
 *
 * Build:
 *   ./build.sh recorder
 *
 * Run on board:
 *   /root/camera_recorder_demo [output_file] [duration_seconds]
 *   /root/camera_recorder_demo /tmp/clip.h264 15
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "camera_recorder.h"

/* Allow Ctrl+C to stop recording gracefully */
static volatile int g_quit = 0;
static void on_signal(int sig) { (void)sig; g_quit = 1; }

int main(int argc, char *argv[])
{
    const char *output  = (argc > 1) ? argv[1] : "/mnt/sdcard/DCIM/Development/recording.h264";
    int         seconds = (argc > 2) ? atoi(argv[2]) : 10;

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("=== Luckfox Pico Mini B — RKMPI Camera Recorder ===\n");
    printf("Output  : %s\n", output);
    printf("Duration: %d seconds (Ctrl+C to stop early)\n\n", seconds);

    /* ── Build config ─────────────────────────────────────────────── */
    RecorderConfig cfg = RECORDER_CONFIG_DEFAULT;
    cfg.output_path  = output;
    cfg.width        = 1920;
    cfg.height       = 1080;
    cfg.fps          = 30;
    cfg.codec        = REC_CODEC_H264;
    cfg.bitrate_kbps = 4096;    /* 4 Mbps — good for 1080p30 */

    /* ── Start recording ──────────────────────────────────────────── */
    if (recorder_start(&cfg) < 0) {
        fprintf(stderr, "Failed to start recorder. "
                        "Did you run RkLunch-stop.sh?\n");
        return EXIT_FAILURE;
    }

    /* ── Wait for the requested duration or a signal ─────────────── */
    for (int i = 0; i < seconds && !g_quit; i++) {
        printf("\r  Recording... %3d / %d s", i + 1, seconds);
        fflush(stdout);
        sleep(1);
    }
    printf("\n");

    /* ── Stop recording ───────────────────────────────────────────── */
    recorder_stop();

    printf("\nDone. File saved to: %s\n", output);
    printf("Play with: ffplay -i %s\n", output);
    return EXIT_SUCCESS;
}
