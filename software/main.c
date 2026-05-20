#include "gpio/gpio.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "recorder/camera_recorder.h"


/* Allow Ctrl+C to stop recording gracefully */
static volatile int g_quit = 0;
static void on_signal(int sig) { (void)sig; g_quit = 1; }

int main() {

  /* 
  --------------------------
  ctrl c graceful exit
  --------------------------
  */
  const char *output  = "/mnt/sdcard/DCIM/Development/recording.h264";
  signal(SIGINT,  on_signal);
  signal(SIGTERM, on_signal);
  RecorderConfig cfg = RECORDER_CONFIG_DEFAULT;


  /*
  --------------------------
  Click detection constants
  Pin: 42 (pin 4 on board)
  --------------------------
  */
  printf("Starting record detection...\n");
  int POLL_MS = 80;
  int button_gpio = 42;


  if (gpio_export(button_gpio) < 0) {
    return -1;
  }
  if (gpio_set_input(button_gpio) < 0) {
    return -1;
  }


  int previous_state = 1;
  int is_recording = 0;

  /* Continuous polling */
 

  printf("\nCleaning up...\n");
  gpio_unexport(button_gpio);

  printf("Done.\n");
  return 0;
}