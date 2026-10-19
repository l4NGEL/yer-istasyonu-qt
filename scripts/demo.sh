#!/usr/bin/env bash
# SITL aracını, sahte video akışını ve yer istasyonunu birlikte başlatır.
#
#   scripts/demo.sh                    # pencereli (X11/Wayland masaüstünde)
#   scripts/demo.sh --screenshot docs/ekran_%1.png --screenshot-tabs 0,1,2
#                                      # ekransız: görüntüleri kaydedip çıkar
#
# Ek argümanlar yer-istasyonu'na aktarılır. BUILD_DIR ile derleme dizini seçilir.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build}"

"${BUILD_DIR}/sitl/gcs-sitl" --gcs 127.0.0.1:14550 --listen 14555 &
SITL_PID=$!
scripts/sim_video.sh 127.0.0.1 5600 &
VIDEO_PID=$!
trap 'kill ${SITL_PID} ${VIDEO_PID} 2>/dev/null || true' EXIT

if [[ " $* " == *" --screenshot "* ]]; then
  # Ekransız çizim: OpenGL gerektirmeyen yazılım arka ucu
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
  export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
fi

"${BUILD_DIR}/app/yer-istasyonu" --port 14550 --video udp://5600 \
  --mission examples/ornek-gorev.plan --demo "$@"
