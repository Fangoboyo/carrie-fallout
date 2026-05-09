/*
 * main.c — Photoresistor reader via SARADC on Luckfox Pico Mini B (RV1103)
 *
 * Reads from the SARADC (IIO) interface, not the digital GPIO sysfs.
 *
 * Wiring:
 *   - One leg of photoresistor → 1.8V
 *   - Other leg → Pin 19 (SARADC_VIN1) AND a 10kΩ pull-down to GND
 *
 * IIO path: /sys/bus/iio/devices/iio:device0/in_voltage1_raw
 *   - ADC resolution : 10-bit (0–1023)
 *   - Reference voltage: 1.8V
 *
 * More light  → lower resistance → higher voltage → higher raw value
 * Less light  → higher resistance → lower voltage  → lower raw value
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Configuration ─────────────────────────────────────────────── */
#define IIO_DEVICE        "/sys/bus/iio/devices/iio:device0"
#define ADC_CHANNEL       "in_voltage1_raw"   /* SARADC_VIN1 = pin 19 */
#define ADC_SCALE_FILE    "in_voltage_scale"  /* mV per LSB            */
#define SAMPLE_INTERVAL_S 1                   /* Seconds between reads  */
#define SAMPLE_COUNT      10                  /* 0 = run forever        */
/* ─────────────────────────────────────────────────────────────── */

/* Read the raw ADC value from the IIO sysfs file.
   Returns the raw integer, or -1 on error. */
static int read_adc_raw(const char *iio_device, const char *channel)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", iio_device, channel);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        perror("Failed to open ADC sysfs file");
        fprintf(stderr, "  Path tried: %s\n", path);
        fprintf(stderr, "  Is the SARADC enabled in your device tree?\n");
        return -1;
    }

    int raw = -1;
    if (fscanf(f, "%d", &raw) != 1) {
        fprintf(stderr, "Failed to read integer from %s\n", path);
        fclose(f);
        return -1;
    }

    fclose(f);
    return raw;
}

/* Read the mV-per-LSB scale factor from the IIO sysfs scale file.
   Returns the scale, or a safe fallback of 1.757812500 mV/LSB. */
static float read_adc_scale(const char *iio_device)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", iio_device, ADC_SCALE_FILE);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "Warning: could not read scale file, using default.\n");
        return 1.757812500f;
    }

    float scale = 1.757812500f;
    fscanf(f, "%f", &scale);
    fclose(f);
    return scale;
}

/* Convert raw ADC value to voltage in mV using the board's scale factor */
static float raw_to_voltage_mv(int raw, float scale_mv_per_lsb)
{
    return (float)raw * scale_mv_per_lsb;
}

/* Simple human-readable light level based on raw value */
static const char *light_level(int raw)
{
    if (raw > 820) return "Very Bright";
    if (raw > 600) return "Bright";
    if (raw > 400) return "Moderate";
    if (raw > 200) return "Dim";
    return "Dark";
}

int main(void)
{
    printf("=== Luckfox Pico Mini B — Photoresistor Reader ===\n");
    printf("ADC path : %s/%s\n", IIO_DEVICE, ADC_CHANNEL);
    printf("Samples  : %s\n",
           SAMPLE_COUNT == 0 ? "continuous (Ctrl+C to stop)" : "10");

    float scale = read_adc_scale(IIO_DEVICE);
    printf("Scale    : %.6f mV/LSB\n", scale);
    printf("\nWiring check — if all values read ~1800 mV, the pin is floating.\n");
    printf("Required: 1.8V -[LDR]- Pin19 -[10k to GND]\n\n");

    printf("%-6s  %-8s  %-10s  %s\n",
           "Sample", "Raw", "Voltage", "Level");
    printf("------  --------  ----------  ----------\n");

    int count = 0;
    while (SAMPLE_COUNT == 0 || count < SAMPLE_COUNT) {
        int raw = read_adc_raw(IIO_DEVICE, ADC_CHANNEL);
        if (raw < 0) {
            /* Error already printed inside read_adc_raw */
            return EXIT_FAILURE;
        }

        float voltage_mv = raw_to_voltage_mv(raw, scale);
        printf("%-6d  %-8d  %-8.2f mV  %s\n",
               count + 1, raw, voltage_mv, light_level(raw));
        fflush(stdout);

        count++;
        if (SAMPLE_COUNT == 0 || count < SAMPLE_COUNT) {
            sleep(SAMPLE_INTERVAL_S);
        }
    }

    printf("\nDone. %d samples read.\n", count);
    return EXIT_SUCCESS;
}