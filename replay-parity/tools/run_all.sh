#!/bin/bash
# replay-parity/tools/run_all.sh OUTDIR [VAR=value ...]
#
# Plays every recording in replay-parity/replays/ (see replay-parity/replays/SOURCE.md) through play_one.sh and
# compares each playback with its recording (mpc_compare.py). A tamper control is compared with
# the recording it was made from. Writes OUTDIR/results.jsonl, one line per recording, and prints
# the summary (summ.py). Run from the repository root with DISC set; extra VAR=value arguments
# go to the game (MELEE_BACKEND=null is the fast way when no pictures are wanted).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
R=$HERE/../replays
OUT=$(readlink -m "$1"); shift
mkdir -p "$OUT"
: > "$OUT/results.jsonl"
for s in "$R"/*.slp; do
  n=$(basename "$s" .slp)
  ref=$s
  case $n in
    *-tampered) ref="$R/${n%-tampered}.slp" ;;
    *-tamper*) ref="$R/${n%%-tamper*}.slp" ;;
  esac
  "$HERE/play_one.sh" "$s" "$OUT/$n" "$@"
  echo "$n $(cat "$OUT/$n/rc.txt")"
  out=$(ls "$OUT/$n"/slp/*.slp 2>/dev/null | head -1)
  if [ -n "$out" ]; then
    python3 "$HERE/mpc_compare.py" "$ref" "$out" |
      python3 -c "import json,sys; r=json.load(sys.stdin); r['recording']='$n'; print(json.dumps(r))" >> "$OUT/results.jsonl"
  else
    echo "{\"recording\": \"$n\", \"error\": \"no playback ($(cat "$OUT/$n/rc.txt"))\"}" >> "$OUT/results.jsonl"
  fi
done
python3 "$HERE/summ.py" "$OUT"
