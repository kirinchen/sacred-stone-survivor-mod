#!/usr/bin/env bash
# 重開後自動:遊戲開始 → 標準遊戲 → 難度確定 → 推劇情(動畫 Esc、對話點右下)N 輪
cd "$(dirname "$0")/.." || exit 1
tools/sss.sh 'click 95 392' >/dev/null; sleep 3
tools/sss.sh 'click 318 170' >/dev/null; sleep 3
tools/sss.sh 'click 283 333' >/dev/null; sleep 3
tools/advance.sh ${1:-60}
