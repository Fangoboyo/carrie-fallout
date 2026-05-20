/*
 * gpio_input.c — Digital GPIO input reader for Luckfox Pico Mini B (RV1103)
 *
 * Shows a menu of every GPIO-capable pin on the board (by physical pin
 * number and GPIO name), lets the user pick one, then continuously reads
 * and prints its digital value (HIGH / LOW) via the Linux sysfs interface.
 *
 * Linux GPIO number = bank*32 + group*8 + bit
 *   Groups: A=0  B=1  C=2  D=3
 *
 * Press Ctrl+C to stop — the pin is unexported cleanly on exit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "gpio.h"

/* ── Poll interval ─────────────────────────────────────────────── */
#define POLL_MS  250          /* milliseconds between reads        */
/* ─────────────────────────────────────────────────────────────── */

#define GPIO_ROOT  "/sys/class/gpio"

/* ── Pin table  (physical pin → gpio name → linux gpio number) ── */
typedef struct {
    int  phys_pin;     /* Physical header pin number          */
    const char *name;  /* Silk-screen GPIO label              */
    int  linux_gpio;   /* Linux GPIO number                   */
    const char *note;  /* Optional extra info (pull, special) */
} PinEntry;

/*
 * All GPIO-capable pins on the Luckfox Pico Mini B.
 * Pins marked SARADC or power-only are omitted.
 *
 * Formula: gpio = bank*32 + group*8 + bit
 *   GPIO1_B2 → 1*32 + 1*8 + 2 = 42
 */
static const PinEntry PINS[] = {
    /*  phys  name          linux_gpio   note                        */
    {   4,  "GPIO1_B2",        42,      "UART2_TX_M1 (alt)"          },
    {   5,  "GPIO1_B3",        43,      "UART2_RX_M1 (alt), pull-up" },
    {   6,  "GPIO1_C0",        48,      "SPI0_CS0_M0 / PWM2_M2"     },
    {   7,  "GPIO1_C1",        49,      "SPI0_CLK_M0 / PWM4_M2"     },
    {   8,  "GPIO1_C2",        50,      "SPI0_MOSI_M0 / PWM5_M2"    },
    {   9,  "GPIO1_C3",        51,      "SPI0_MISO_M0 / PWM6_M2"    },
    {  10,  "GPIO1_C4",        52,      "UART4_RX_M1 / PWM8_M1"     },
    {  11,  "GPIO1_C5",        53,      "UART4_TX_M1 / PWM9_M1"     },
    {  12,  "GPIO1_D0",        56,      "UART3_TX_M1"               },
    {  13,  "GPIO1_D1",        57,      "UART3_RX_M1"               },
    {  14,  "GPIO1_D2",        58,      "I2C3_SDA_M1 / SPI0_CS1_M0" },
    {  15,  "GPIO1_D3",        59,      "I2C3_SCL_M1"               },
    {  16,  "GPIO1_C6",        54,      "UART4_RTS_M1"              },
    {  17,  "GPIO1_C7",        55,      "UART4_CTS_M1"              },
    {  18,  "GPIO0_A4",         4,      ""                           },
    {  20,  "GPIO4_C1",       145,      "SARADC_IN1 (digital mode)"  },
    {  21,  "GPIO4_C0",       144,      "SARADC_IN0 (digital mode)"  },
    /* On-board buttons / LEDs                                         */
    {   0,  "GPIO1_A2",        34,      "USER button (active-low)"   },
    {   0,  "GPIO3_C6",       118,      "ACT LED"                    },
};

#define PIN_COUNT  ((int)(sizeof(PINS) / sizeof(PINS[0])))

/* ── Signal handling ─────────────────────────────────────────── */
static volatile int keep_running = 1;
static void handle_sigint(int sig) { (void)sig; keep_running = 0; }


/* ── UI helpers ─────────────────────────────────────────────── */

static void print_header(void)
{
    printf("\033[2J\033[H");   /* clear screen */
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║       Luckfox Pico Mini B  —  GPIO Input Reader      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
}

static void print_pinout(void)
{
    /* Compact ASCII pinout matching the board's left/right header */
    printf("  ┌─── Left header ────┐   ┌─── Right header ───┐\n");
    printf("  │ 1  VBUS            │   │ 22  1V8(OUT)        │\n");
    printf("  │ 2  GND             │   │ 23  GND             │\n");
    printf("  │ 3  3V3(OUT)        │   │ 20  GPIO4_C1 (ADC1) │\n");
    printf("  │ 4  GPIO1_B2        │   │ 21  GPIO4_C0 (ADC0) │\n");
    printf("  │ 5  GPIO1_B3 (↑)    │   │ 18  GPIO0_A4        │\n");
    printf("  │ 6  GPIO1_C0        │   │ 17  GPIO1_C7        │\n");
    printf("  │ 7  GPIO1_C1        │   │ 16  GPIO1_C6        │\n");
    printf("  │ 8  GPIO1_C2        │   │ 15  GPIO1_D3        │\n");
    printf("  │ 9  GPIO1_C3        │   │ 14  GPIO1_D2        │\n");
    printf("  │ 10 GPIO1_C4        │   │ 13  GPIO1_D1        │\n");
    printf("  │ 11 GPIO1_C5        │   │ 12  GPIO1_D0        │\n");
    printf("  └────────────────────┘   └─────────────────────┘\n");
    printf("  On-board:  USER button = GPIO1_A2   ACT LED = GPIO3_C6\n\n");
}

static void print_menu(void)
{
    printf("  #   Phys  GPIO Name    Linux#  Notes\n");
    printf("  ─   ────  ──────────   ──────  ─────────────────────────\n");
    for (int i = 0; i < PIN_COUNT; i++) {
        const PinEntry *p = &PINS[i];
        const char *phys = (p->phys_pin > 0) ? "" : "(on-board)";
        if (p->phys_pin > 0)
            printf("  %-2d  [%2d]  %-12s  %3d     %s\n",
                   i + 1, p->phys_pin, p->name, p->linux_gpio, p->note);
        else
            printf("  %-2d  %-5s %-12s  %3d     %s\n",
                   i + 1, phys, p->name, p->linux_gpio, p->note);
    }
    printf("\n");
}

/* ── main ───────────────────────────────────────────────────── */

int main(void)
{
    signal(SIGINT, handle_sigint);

    print_header();
    print_pinout();
    print_menu();

    printf("Select a pin (1-%d), or enter a raw GPIO number (0-511): ", PIN_COUNT);
    fflush(stdout);

    int choice;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Invalid input.\n");
        return EXIT_FAILURE;
    }

    const PinEntry *selected = NULL;
    PinEntry custom = {0};

    if (choice >= 1 && choice <= PIN_COUNT) {
        /* Named pin from menu */
        selected = &PINS[choice - 1];
    } else if (choice >= 0 && choice <= 511) {
        /* Raw Linux GPIO number */
        custom.phys_pin  = -1;
        custom.linux_gpio = choice;
        snprintf((char *)(custom.note = ""), 0, "");
        /* Build name from number */
        static char custom_name[32];
        int bank  = choice / 32;
        int rest  = choice % 32;
        int group = rest / 8;
        int bit   = rest % 8;
        snprintf(custom_name, sizeof(custom_name), "GPIO%d_%c%d",
                 bank, "ABCD"[group], bit);
        custom.name = custom_name;
        custom.note = "(custom)";
        selected = &custom;
    } else {
        fprintf(stderr, "Out of range.\n");
        return EXIT_FAILURE;
    }

    printf("\n→ Using %s  (linux gpio %d)\n\n",
           selected->name, selected->linux_gpio);

    /* --- Export + configure ------------------------------------ */
    if (gpio_export(selected->linux_gpio) < 0)
        return EXIT_FAILURE;
    if (gpio_set_input(selected->linux_gpio) < 0) {
        gpio_unexport(selected->linux_gpio);
        return EXIT_FAILURE;
    }

    /* --- Print live table header ------------------------------ */
    printf("Polling %s every %d ms  —  Ctrl+C to stop\n\n",
           selected->name, POLL_MS);
    printf("  %-6s  %-6s  %-6s  %s\n", "Sample", "Value", "State", "Edge");
    printf("  ──────  ──────  ──────  ──────────\n");

    int count    = 0;
    int prev_val = -1;

    while (keep_running) {
        int val = gpio_read(selected->linux_gpio);
        if (val < 0) {
            fprintf(stderr, "  [!] Read error — aborting.\n");
            gpio_unexport(selected->linux_gpio);
            return EXIT_FAILURE;
        }

        const char *state  = val ? "\033[32mHIGH\033[0m" : "\033[33mLOW \033[0m";
        const char *edge   = "";
        if (prev_val != -1 && val != prev_val)
            edge = val ? "↑ RISING" : "↓ FALLING";

        printf("  %-6d  %-6d  %s  %s\n", count + 1, val, state, edge);
        fflush(stdout);

        prev_val = val;
        count++;
        usleep(POLL_MS * 1000);
    }

    printf("\n\nStopped after %d samples. Unexporting gpio%d...\n",
           count, selected->linux_gpio);
    gpio_unexport(selected->linux_gpio);
    printf("Done.\n");
    return EXIT_SUCCESS;
}
