#!/usr/bin/env bash
# 自動探索:隨機方向按住走 + 動畫按 Esc + 點對話;敵人 >= ${2:-1} 連續兩次就停
cd "$(dirname "$0")/.." || exit 1
dirs=("600 240" "560 380" "320 400" "80 380" "40 240" "80 80" "320 60" "560 80")
hit=0
for i in $(seq 1 ${1:-60}); do
  l=$(grep "fps=" game/sss_mod.log | tail -1)
  n=$(echo "$l" | grep -o 'enemy=[0-9]*' | cut -d= -f2)
  if [ "${n:-0}" -ge "${2:-1}" ]; then hit=$((hit+1)); [ $hit -ge 2 ] && { echo "STOP $i: $l"; exit 0; }; sleep 1.2; continue; fi
  hit=0
  if echo "$l" | grep -q "fps=2[3-5] "; then tools/sss.sh 'key 27 120' >/dev/null
  else tools/sss.sh 'click 600 455' >/dev/null; sleep 0.4; tools/sss.sh "hold ${dirs[$((RANDOM % 8))]} 1800" >/dev/null; fi
  sleep 2.2
done
echo "END: $(grep 'fps=' game/sss_mod.log | tail -1)"
