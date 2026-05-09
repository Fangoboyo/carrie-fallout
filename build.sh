#!/bin/bash
set -e

TOOLCHAIN="arm-rockchip830-linux-uclibcgnueabihf-gcc"

# ── SDK / RKMPI paths — set as ENV in Dockerfile, no manual export needed ─
# LUCKFOX_SDK_PATH = /opt/luckfox-pico    (toolchain root)
# RKMPI_ROOT       = /opt/luckfox-rkmpi   (headers + uclibc libs)
ROCKIT_INC="${RKMPI_ROOT}/include"
ROCKIT_LIB="${RKMPI_ROOT}/lib/uclibc"
echo "[build] RKMPI_ROOT = ${RKMPI_ROOT}"

# ── Target selection ─────────────────────────────────────────────────
TARGET="${1:-main}"

case "${TARGET}" in
  main)
    echo "[build] Compiling main (ADC photoresistor reader)..."
    ${TOOLCHAIN} main.c -o main
    adb -H host.docker.internal push main /root/
    adb -H host.docker.internal shell "chmod +x /root/main && /root/main"
    ;;

  recorder)
    echo "[build] Compiling camera_recorder_demo (RKMPI 1080p H.264)..."
    ${TOOLCHAIN} \
      -I"${ROCKIT_INC}" \
      camera_recorder.c camera_recorder_demo.c \
      -o camera_recorder_demo \
      -L"${ROCKIT_LIB}" \
      -lrockit -lpthread \
      -Wl,-rpath-link,"${ROCKIT_LIB}"

    adb -H host.docker.internal push camera_recorder_demo /root/
    adb -H host.docker.internal shell "chmod +x /root/camera_recorder_demo"
    echo "[build] Pushed. Run on board:  adb shell /root/camera_recorder_demo"
    ;;

  *)
    echo "Usage: $0 [main|recorder]"
    exit 1
    ;;
esac