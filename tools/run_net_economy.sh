#!/bin/bash
# Launch two packaged clients on loopback running the networked economy fixture.
# usage: tools/run_net_economy.sh <out-dir> <session> <ticks> [seed]
set -u
OUT=$(cd "$1" 2>/dev/null && pwd -W || { mkdir -p "$1"; cd "$1" && pwd -W; })
SESSION=$2; TICKS=$3; SEED=${4:-7}
PKG="$(cd "$(dirname "$0")/.." && pwd)/artifacts/package/Voidfront.exe"
for p in 0 1; do
  q=$((1-p))
  ( "$PKG" --log-file "$OUT/p$p-engine.log" --resolution 1280x720 -- --network-economy-smoke \
      --ticks=$TICKS --player=$p --port=$((43000+p)) --remote-port=$((43000+q)) --session=$SESSION --seed=$SEED \
      --capture="$OUT/p$p.png" --report="$OUT/p$p.json" > "$OUT/p$p.out" 2>&1; echo "exit $?" >> "$OUT/p$p.out" ) &
done
wait
grep -h "NETECON \|^exit\|NETWORK.*error" "$OUT"/p0.out "$OUT"/p1.out
