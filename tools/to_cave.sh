#!/usr/bin/env bash
# 重開 → 劇情 → 琉璃仙境往右下走 → 推劇情 → 自動探索到有原生敵人的地圖(琉璃仙洞)
cd "$(dirname "$0")/.." || exit 1
tools/relaunch.sh || exit 1
tools/to_story.sh 90 | tail -1
for i in 1 2 3 4 5 6; do tools/sss.sh 'hold 600 340 3000' >/dev/null; sleep 3.5; grep fps= game/sss_mod.log | tail -1 | grep -q "obj=2 " && break; done
tools/advance.sh 40 | tail -1
tools/explore.sh 120
