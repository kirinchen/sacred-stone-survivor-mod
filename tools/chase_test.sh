#!/usr/bin/env bash
# 追擊方式對照:tools/chase_test.sh MODE [秒=40] [RMIN=260] [RMAX=380] [其他指令...]
# PRE='goto 330 150' = 開始前先下的指令(例:先把主角走到路中間)
# 每次重新讀檔 → 不升級、自動出手 → 遠處生怪 → 結束時印 CHASE 統計(到主角 60px 內的平均秒數、卡住數)。
cd "$(dirname "$0")/.." || exit 1
m=$1; s=${2:-40}; r0=${3:-260}; r1=${4:-380}; shift 4 2>/dev/null
tools/load.sh >/dev/null 2>&1; sleep 2
tools/sss.sh 'survxp 0' "survr $r0 $r1" "survchase $m" "$@" >/dev/null
[ -n "$PRE" ] && { tools/sss.sh "$PRE" >/dev/null; sleep ${PRE_WAIT:-5}; }
tools/sss.sh surv >/dev/null; sleep "$s"
tools/sss.sh surv >/dev/null
grep -a -E "^.{13}(chase m|CHASE|SURV end|ai def|chase:)" game/sss_mod.log | tail -n 6 | cut -c1-220
