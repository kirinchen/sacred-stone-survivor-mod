#!/usr/bin/env bash
# 腳本代玩一局 Roguelite:tools/rogue_test.sh 地圖編號 最多秒數 [開局後的指令...]
#   例:tools/rogue_test.sh 0 700                     → 正常速度玩一局(10 分鐘)
#       tools/rogue_test.sh 0 120 'survtscale 6'      → 時鐘 6 倍速(測精英 / 頭目 / 結算)
# 主角繞著進場點(地圖中央)走;升級選單出現就選(優先「學新絕招」4,不然輪流)。結束印 ROGUE / v1 end 幾行。
cd "$(dirname "$0")/.." || exit 1
m=$1; s=$2; shift 2
L=game/sss_mod.log
e0=$(grep -a -c "ROGUE end" $L); n0=$(grep -a -c "LEVELUP" $L); r0=$(grep -a -c "ROGUE run started" $L); k=0
[ -n "$NOSTART" ] || echo "rogue $m" > game/sss_cmd.txt
if [ "$NOSTART" != 2 ]; then      # NOSTART=2:接手已經在跑的一局
for i in $(seq 1 60); do sleep 0.5; [ "$(grep -a -c 'ROGUE run started' $L)" -gt "$r0" ] && break; done
[ "$(grep -a -c 'ROGUE run started' $L)" -gt "$r0" ] || { echo "run did not start"; grep -a -E "ROGUE|goto_map" $L | tail -4; exit 1; }
fi
c=$(grep -a "centre (" $L | tail -1 | sed 's/.*centre (\([0-9]*\),\([0-9]*\)).*/\1 \2/'); cx=${c% *}; cy=${c#* }
[ -n "$cx" ] || { cx=640; cy=480; }
[ $# -gt 0 ] && { printf '%s\n' "$@" > game/sss_cmd.txt; sleep 0.6; }
[ -n "$REC" ] && tools/rec.sh start >/dev/null
R=${R:-110}
for i in $(seq 1 $(( s * 2 ))); do
  sleep 0.5
  [ "$(grep -a -c 'ROGUE end' $L)" -gt "$e0" ] && break
  n=$(grep -a -c "LEVELUP" $L)
  if [ "$n" -gt "$n0" ]; then
    n0=$n; k=$((k+1)); sleep ${MENU_WAIT:-0.7}
    opts=$(grep -a "LEVELUP" $L | tail -1 | sed 's/.*options=//'); p=$(( (k % 3) + 1 ))
    j=0; for o in ${opts//,/ }; do j=$((j+1)); for want in ${PREFER:-4}; do [ "$o" = "$want" ] && p=$j; done; done
    echo "pick $p" > game/sss_cmd.txt; sleep 0.3; continue
  fi
  if [ -z "$NOMOVE" ] && [ $((i % 4)) = 0 ]; then      # NOMOVE=1:不下走路指令(原作操作 + bot 1 時,走路交給原作的普攻指令)
    case $(( (i / 4) % 4 )) in 0) dx=$R; dy=0;; 1) dx=0; dy=$((R*6/10));; 2) dx=-$R; dy=0;; 3) dx=0; dy=-$((R*6/10));; esac
    echo "goto $((cx+dx)) $((cy+dy))" > game/sss_cmd.txt
  fi
done
[ -n "$REC" ] && { sleep 3; tools/rec.sh stop ${REC_ITEM:-first} "$REC"; }
grep -a -E "ROGUE (run|elite|boss|end)|v1 end|SURV end|exception|fault" $L | tail -${TAIL:-12} | cut -c1-250
