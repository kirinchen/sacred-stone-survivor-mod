#!/usr/bin/env bash
# 重建 → 重開 → 標題「進度讀取」→ 自動存檔。約 25 秒。
# 自動存檔會在換地圖時被原作覆寫 → 每次先還原備份(game/Saves/exsave99.corridor.bak:琉璃仙境通道,洞口外)。
cd "$(dirname "$0")/.." || exit 1
taskkill //IM WUJIN.exe //F >/dev/null 2>&1; sleep 1
[ -f game/Saves/exsave99.corridor.bak ] && cp game/Saves/exsave99.corridor.bak game/Saves/exsave99.sav
tools/relaunch.sh 14 || exit 1
tools/sss.sh 'click 205 392' >/dev/null; sleep 2
tools/sss.sh 'click 230 295' >/dev/null; sleep 1
tools/sss.sh 'click 450 333' >/dev/null; sleep 6
grep fps= game/sss_mod.log | tail -1
