#!/usr/bin/env bash
# 邊砍邊走對照:tools/atk_test.sh MODE [秒=30] [其他指令...]   MODE = survatk 的值(0 原作站定 / 1 模組結算 / 2 原作揮刀 + 滑行)
# 每次重新讀檔 → 不升級、命鎖滿 → 主角每 2.5 秒在 (320,330) / (320,620) 之間來回走 → 印 v1 end 統計(走了幾 px、幾成時間在走)。
# REC=名字 → 同時用 OBS 錄(先 pwsh tools/obs_session.ps1 -Begin),存成 compare-<名字>.mp4
cd "$(dirname "$0")/.." || exit 1
m=$1; s=${2:-30}; shift 2 2>/dev/null
tools/load.sh >/dev/null 2>&1; sleep 2
tools/sss.sh 'survxp 0' 'god 1' 'survr 120 220' "survatk $m" "$@" 'goto 320 480' >/dev/null; sleep 5
[ -n "$REC" ] && tools/rec.sh start >/dev/null
tools/sss.sh surv >/dev/null; sleep 3
n=$(( (s - 3) / 5 ))
for i in $(seq 1 $n); do tools/sss.sh 'goto 320 330' >/dev/null; sleep 2.5; tools/sss.sh 'goto 320 620' >/dev/null; sleep 2.5; done
[ -n "$SHOT" ] && tools/sss.sh shot "$SHOT" >/dev/null
tools/sss.sh surv >/dev/null
[ -n "$REC" ] && tools/rec.sh stop compare "$REC"
grep -a -E "v1 end|SURV end|swing:" game/sss_mod.log | tail -n 2 | cut -c14-260
