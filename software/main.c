#include "gpio/gpio.h"
#include <stdio.h>
#include <unistd.h>

int main() {
  printf("Starting record detection...\n");
  int POLL_MS = 50;
  int button_gpio = 42;

  int previous_state = 1;
  int is_recording = 0;

  /* Continuous polling */
  while (1) {
    int input = gpio_read(button_gpio);

    if (input != previous_state) {
      is_recording = !is_recording;
      printf("\r  Recording state: %d", is_recording);
      fflush(stdout);
      previous_state = input;
    }

    usleep(POLL_MS * 1000);
  }

  printf("\nCleaning up...\n");
  gpio_unexport(button_gpio);

  printf("Done.\n");
  return 0;
}