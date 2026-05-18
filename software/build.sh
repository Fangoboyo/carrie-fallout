#!/bin/bash
set -e

# Project Abbreviations:
# - RKMPI: Rockchip Media Process Interface. An API used for multimedia processing (e.g., video encoding/decoding) on Rockchip processors.
# - RKAIQ: Rockchip AI Image Quality. An Image Signal Processing (ISP) framework for tuning and image enhancement on Rockchip camera platforms.
# - RV1103: A low-power, high-performance Vision SoC (System-on-Chip) from Rockchip designed for IP cameras, featuring an integrated NPU and ISP.

TOOLCHAIN="arm-rockchip830-linux-uclibcgnueabihf-gcc"
# RKMPI_ROOT       = /opt/luckfox-rkmpi   (headers + uclibc libs)
ROCKIT_INC="${RKMPI_ROOT}/include"
RKAIQ_INC="${RKMPI_ROOT}/include/rkaiq"
ROCKIT_LIB="${RKMPI_ROOT}/lib/uclibc"
echo "[build] RKMPI_ROOT = ${RKMPI_ROOT}"

TARGET="${1:-main}"

case "${TARGET}" in
  recorder)
    cd recorder
    echo "[build] Compiling camera_recorder_demo (RKMPI 1080p H.264)..."
    ${TOOLCHAIN} \
      -I"${ROCKIT_INC}" \
      -I"${RKAIQ_INC}" \
      -I"${RKAIQ_INC}/uAPI2" \
      -I"${RKAIQ_INC}/common" \
      -I"${RKAIQ_INC}/xcore" \
      -I"${RKAIQ_INC}/algos" \
      -I"${RKAIQ_INC}/iq_parser" \
      -I"${RKAIQ_INC}/iq_parser_v2" \
      camera_recorder.c camera_recorder_demo.c \
      -o camera_recorder_demo \
      -L"${ROCKIT_LIB}" \
      -lrockit -lsample_comm -lrkaiq -lpthread \
      -Wl,-rpath-link,"${ROCKIT_LIB}"

    adb push camera_recorder_demo /root/
    adb shell "chmod +x /root/camera_recorder_demo"
    rm camera_recorder_demo
    echo "[build] Pushed. Run on board:  adb shell /root/camera_recorder_demo"
    ;;

  gpio)
    cd gpio
    echo "[build] Compiling gpio_input (digital GPIO reader)..."
    ${TOOLCHAIN} gpio_input.c gpio.c -o gpio_input
    adb push gpio_input /root/
    adb shell "chmod +x /root/gpio_input && /root/gpio_input"
    rm gpio_input
    ;;

  main)
    echo "[build] Compiling main (RKMPI 1080p H.264)..."
    ${TOOLCHAIN} \
      -I"${ROCKIT_INC}" \
      -I"${RKAIQ_INC}" \
      -I"${RKAIQ_INC}/uAPI2" \
      -I"${RKAIQ_INC}/common" \
      -I"${RKAIQ_INC}/xcore" \
      -I"${RKAIQ_INC}/algos" \
      -I"${RKAIQ_INC}/iq_parser" \
      -I"${RKAIQ_INC}/iq_parser_v2" \
      main.c gpio/gpio.c recorder/camera_recorder.c \
      -o main \
      -L"${ROCKIT_LIB}" \
      -lrockit -lsample_comm -lrkaiq -lpthread \
      -Wl,-rpath-link,"${ROCKIT_LIB}"

    adb push main /root/
    adb shell "chmod +x /root/main"
    rm main
    echo "[build] Pushed. Run on board:  adb shell /root/main"
    ;;

  *)
    echo "Usage: $0 [recorder|gpio]"
    exit 1
    ;;
esac