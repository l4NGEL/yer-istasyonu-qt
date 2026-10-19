#!/usr/bin/env bash
# Sahte faydalı yük kamerası: test desenini H.264'e kodlayıp RTP/UDP ile yollar
# (QGroundControl'ün varsayılanı olan 5600 portu).
#
#   scripts/sim_video.sh [host] [port]
#
# Gereken eklentiler: gstreamer1.0-plugins-{base,good,ugly}. Pango eklentisi
# (timeoverlay/textoverlay) kuruluysa görüntüye saat ve etiket yazılır.
set -euo pipefail
HOST="${1:-127.0.0.1}"
PORT="${2:-5600}"

OVERLAY=""
if gst-inspect-1.0 textoverlay >/dev/null 2>&1; then
  OVERLAY='! timeoverlay halignment=right valignment=bottom font-desc="Sans 20"
           ! textoverlay text="SITL FAYDALI YUK KAMERASI" valignment=top halignment=left font-desc="Sans 18"'
fi

# shellcheck disable=SC2086
eval exec gst-launch-1.0 -q \
  videotestsrc is-live=true pattern=ball motion=sweep background-color=0xff20303a \
  ! video/x-raw,width=1280,height=720,framerate=30/1 \
  ${OVERLAY} \
  ! videoconvert \
  ! x264enc tune=zerolatency speed-preset=ultrafast bitrate=2500 key-int-max=30 \
  ! rtph264pay config-interval=1 pt=96 \
  ! udpsink host="${HOST}" port="${PORT}" sync=false
