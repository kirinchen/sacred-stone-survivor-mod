#!/usr/bin/env bash
# 自動試玩:升級選單一出現就選(預設輪流 1/2/3;PICK=固定選第幾個),直到結算或 N 秒。主角不移動,全靠自動出手。
cd "$(dirname "$0")/.." || exit 1
n0=$(grep -a -c "LEVELUP" game/sss_mod.log); e0=$(grep -a -c "SURV end" game/sss_mod.log); k=0
for i in $(seq 1 $(( ${1:-120} * 2 ))); do
  sleep 0.5
  [ "$(grep -a -c 'SURV end' game/sss_mod.log)" -gt "$e0" ] && break
  n=$(grep -a -c "LEVELUP" game/sss_mod.log)
  if [ "$n" -gt "$n0" ]; then n0=$n; k=$((k+1)); sleep ${MENU_WAIT:-1}; echo "pick ${PICK:-$(( (k % 3) + 1 ))}" > game/sss_cmd.txt; fi
done
grep -a -E "SURV (start|end)|v1 end" game/sss_mod.log | tail -3
