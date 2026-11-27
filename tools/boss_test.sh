#!/usr/bin/env bash
# 單獨測頭目戰:tools/boss_test.sh 地圖 攻 防 命 [開局後的指令...]
# 開一局 → 主角設成指定強度(一般打到 8:00 的樣子,例 41 25 600)→ 跳到 7:55 → 代玩到結束(最多 240 秒)。印頭目出場到結束花了幾秒。
cd "$(dirname "$0")/.." || exit 1
m=$1; a=$2; d=$3; h=$4; shift 4
NOSTART= TAIL=1 tools/rogue_test.sh $m 2 >/dev/null 2>&1
tools/sss.sh "rgstat $a $d $h" "$@" >/dev/null; sleep 0.5; tools/sss.sh 'survtime 475' >/dev/null
NOSTART=2 TAIL=6 tools/rogue_test.sh $m 240 2>/dev/null | grep -a -E "ROGUE (boss|end)|hit cap" | cut -c1-200
