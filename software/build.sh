#!/bin/bash
set -e

TOOLCHAIN="arm-rockchip830-linux-uclibcgnueabihf-gcc"
# RKMPI_ROOT       = /opt/luckfox-rkmpi   (headers + uclibc libs)
ROCKIT_INC="${RKMPI_ROOT}/include"
RKAIQ_INC="${RKMPI_ROOT}/include/rkaiq"
ROCKIT_LIB="${RKMPI_ROOT}/lib/uclibc"
echo "[build] RKMPI_ROOT = ${RKMPI_ROOT}"

TARGET="${1:-main}"

case "${TARGET}" in
  recorder)
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

    adb -H host.docker.internal push camera_recorder_demo /root/
    adb -H host.docker.internal shell "chmod +x /root/camera_recorder_demo"
    rm camera_recorder_demo
    echo "[build] Pushed. Run on board:  adb shell /root/camera_recorder_demo"
    ;;

  gpio)
    echo "[build] Compiling gpio_input (digital GPIO reader)..."
    ${TOOLCHAIN} gpio_input.c -o gpio_input
    adb -H host.docker.internal push gpio_input /root/
    adb -H host.docker.internal shell "chmod +x /root/gpio_input && /root/gpio_input"
    rm gpio_input
    ;;

  *)
    echo "Usage: $0 [recorder|gpio]"
    exit 1
    ;;
esac