/*
 * Project Abbreviations:
 * - RKMPI: Rockchip Media Process Interface. An API used for multimedia processing (e.g., video encoding/decoding) on Rockchip processors.
 * - RKAIQ: Rockchip AI Image Quality. An Image Signal Processing (ISP) framework for tuning and image enhancement on Rockchip camera platforms.
 * - RV1103: A low-power, high-performance Vision SoC (System-on-Chip) from Rockchip designed for IP cameras, featuring an integrated NPU and ISP.
 */

#include "gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define GPIO_ROOT  "/sys/class/gpio"

static int sysfs_write(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "  [!] Cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }
    fprintf(f, "%s", value);
    fclose(f);
    return 0;
}

int gpio_export(int gpio)
{
    char dir[80];
    snprintf(dir, sizeof(dir), GPIO_ROOT "/gpio%d", gpio);
    if (access(dir, F_OK) == 0) return 0;   /* already exported */

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", gpio);
    if (sysfs_write(GPIO_ROOT "/export", buf) < 0) {
        fprintf(stderr, "  [!] Export failed — is gpio%d valid on this kernel?\n", gpio);
        return -1;
    }
    return 0;
}

void gpio_unexport(int gpio)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", gpio);
    sysfs_write(GPIO_ROOT "/unexport", buf);
}

int gpio_set_input(int gpio)
{
    char path[80];
    snprintf(path, sizeof(path), GPIO_ROOT "/gpio%d/direction", gpio);

    /* Wait up to 200 ms for the directory to appear after export */
    for (int i = 0; i < 10; i++) {
        if (access(path, F_OK) == 0) break;
        usleep(20000);
    }
    if (sysfs_write(path, "in") < 0) {
        fprintf(stderr, "  [!] Could not set gpio%d direction to 'in'\n", gpio);
        return -1;
    }
    return 0;
}

int gpio_read(int gpio)
{
    char path[80];
    snprintf(path, sizeof(path), GPIO_ROOT "/gpio%d/value", gpio);
    FILE *f = fopen(path, "r");
    if (!f) { perror("  [!] read value"); return -1; }
    
    int v = -1;
    fscanf(f, "%d", &v);
    fclose(f);
    return v;
}
