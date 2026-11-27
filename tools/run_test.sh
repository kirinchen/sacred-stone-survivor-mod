#!/usr/bin/env bash
# 正常玩一輪(會升級、會死):tools/run_test.sh 秒 X1 Y1 X2 Y2 [開始前的指令...]
# 主角在 (X1,Y1) / (X2,Y2) 之間來回走,升級選單出現就選(優先選「學新絕招」= 看 log 的 options 裡有 4 就選它,不然輪流)。
cd "$(dirname "$0")/.." || exit 1
s=$1; x1=$2; y1=$3; x2=$4; y2=$5; shift 5
[ $# -gt 0 ] && { tools/sss.sh "$@" >/dev/null; sleep ${PRE_WAIT:-1}; }
n0=$(grep -a -c "LEVELUP" game/sss_mod.log); e0=$(grep -a -c "SURV end" game/sss_mod.log); k=0
[ -n "$REC" ] && tools/rec.sh start >/dev/null
tools/sss.sh surv >/dev/null
for i in $(seq 1 $(( s * 2 ))); do
  sleep 0.5
  [ "$(grep -a -c 'SURV end' game/sss_mod.log)" -gt "$e0" ] && break
  n=$(grep -a -c "LEVELUP" game/sss_mod.log)
  if [ "$n" -gt "$n0" ]; then
    n0=$n; k=$((k+1)); sleep ${MENU_WAIT:-1.2}
    opts=$(grep -a "LEVELUP" game/sss_mod.log | tail -1 | sed 's/.*options=//'); p=$(( (k % 3) + 1 ))
    j=0; for o in ${opts//,/ }; do j=$((j+1)); [ "$o" = 4 ] && p=$j; done
    echo "pick $p" > game/sss_cmd.txt; sleep 0.4; continue
  fi
  [ $((i % 5)) = 0 ] && { if [ $(( (i / 5) % 2 )) = 0 ]; then echo "goto $x1 $y1" > game/sss_cmd.txt; else echo "goto $x2 $y2" > game/sss_cmd.txt; fi; }
done
[ "$(grep -a -c 'SURV end' game/sss_mod.log)" -gt "$e0" ] || tools/sss.sh surv >/dev/null
[ -n "$REC" ] && { sleep 2; tools/rec.sh stop ${REC_ITEM:-first} "$REC"; }
grep -a -E "SURV (start|end)|v1 end|forget|learn move|goto_map: stage" game/sss_mod.log | tail -8 | cut -c14-260
