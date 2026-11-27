#!/usr/bin/env bash
# 推劇情 N 輪;出現敵人(enemy>0)就停
cd "$(dirname "$0")/.." || exit 1
for i in $(seq 1 ${1:-40}); do
  l=$(grep "fps=" game/sss_mod.log | tail -1)
  echo "$l" | grep -q "enemy=[1-9]" && { echo "ENEMY: $l"; exit 0; }
  if echo "$l" | grep -q "fps=2[3-5] "; then tools/sss.sh 'key 27 120' >/dev/null; else tools/sss.sh 'click 600 455' >/dev/null; fi
  sleep 1.5
done
grep "fps=" game/sss_mod.log | tail -1
