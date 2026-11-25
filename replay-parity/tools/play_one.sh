#!/bin/bash
# replay-parity/tools/play_one.sh SLP OUTDIR [VAR=value ...]
#
# Plays one Slippi recording through this build's MELEE_SLP_PLAY and records the playback with
# MELEE_SLP_DIR into OUTDIR/slp/. Runs under a private Xvfb, with the network unshared when
# `unshare -rn` is available, and per-run data folders under OUTDIR.
#
# Environment:
#   DISC      your Melee NTSC 1.02 disc image (required)
#   MELEE     the melee binary (default: build/melee)
#   TMO       timeout in seconds (default 1800)
# Extra VAR=value arguments are passed to the game, e.g. MELEE_BACKEND=null (fast, no pictures)
# or MELEE_SLP_DUMP=OUTDIR/dump MELEE_SLP_DUMP_SCALE=0.5 (needs a real renderer).
set -u
SLP=$(readlink -f "$1"); O=$(readlink -m "$2"); shift 2
: "${DISC:?set DISC to your NTSC 1.02 disc image}"
BIN=$(readlink -f "${MELEE:-build/melee}")
rm -rf "$O"; mkdir -p "$O/xdg" "$O/slp" "$O/cache"
for D in $(seq 60 99); do [ -e /tmp/.X11-unix/X$D ] || [ -e /tmp/.X$D-lock ] || break; done
Xvfb :$D -screen 0 1024x768x24 -nolisten tcp > "$O/xvfb.log" 2>&1 &
XP=$!
for i in $(seq 50); do [ -e /tmp/.X11-unix/X$D ] && break; sleep 0.1; done
NET=""
if unshare -rn true 2>/dev/null; then NET="unshare -rn"; fi
env -i HOME="$HOME" PATH=/usr/bin:/bin DISPLAY=:$D \
  XDG_DATA_HOME="$O/xdg" XDG_CONFIG_HOME="$O/xdg" XDG_CACHE_HOME="$O/cache" \
  SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy \
  MELEE_VSYNC=0 MELEE_UCF=1 MELEE_BOOT_SCENE=vs MELEE_SLP_PLAY="$SLP" MELEE_SLP_DIR="$O/slp" \
  MELEE_LOG_FILE="$O/port.log" MELEE_FILES_DIR="$O/nonexistent" MELEE_CACHE_DIR="$O/cache" \
  "$@" \
  timeout "${TMO:-1800}" $NET nice -n 10 "$BIN" --no-card "$DISC" > "$O/stdout.txt" 2>&1
echo "rc=$?" > "$O/rc.txt"
kill $XP 2>/dev/null; wait $XP 2>/dev/null
rm -rf "$O/xdg" "$O/cache"
