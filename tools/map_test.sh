#!/usr/bin/env bash
# 逐張測 goto_map:tools/map_test.sh ID...  → 每張等 WAIT 秒(預設 4)後印 where 結果;遊戲死了就重新讀檔並記 CRASH。
# 輸出一行一張:id <TAB> 結果(到了 / 沒到 / CRASH)<TAB> where 內容。先 tools/load.sh。
cd "$(dirname "$0")/.." || exit 1
W=${WAIT:-4}
for id in "$@"; do
  printf 'goto_map %s\n' "$id" > game/sss_cmd.txt
  sleep "$W"
  if ! tasklist 2>/dev/null | grep -qi WUJIN.exe; then
    printf '%s\tCRASH\t%s\n' "$id" "$(grep -a goto_map game/sss_mod.log | tail -1 | cut -c14-)"
    tools/load.sh >/dev/null 2>&1; sleep 3; continue
  fi
  echo where > game/sss_cmd.txt; sleep 0.8
  w=$(grep -a 'where:' game/sss_mod.log | tail -1 | cut -c14-)
  last=$(tail -c 300 game/sss_mod.log | grep -a -c 'fps=')
  case "$w" in
    *"stage $id "*) r=OK ;;
    *"stage $((id + 200)) "*) r="OK(+200)" ;;
    *) r="NOT-ARRIVED($(grep -a 'goto_map:' game/sss_mod.log | tail -1 | cut -c24-70))" ;;
  esac
  printf '%s\t%s\t%s\n' "$id" "$r" "$w"
done
