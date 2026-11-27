#!/usr/bin/env bash
# 對注入中的遊戲送指令:tools/sss.sh 'spawn 50' 'dump' ...;tools/sss.sh shot NAME → shots/local/NAME.png
cd "$(dirname "$0")/.." || exit 1
if [ "$1" = shot ]; then
  n=${2:-shot}; rm -f "game/$n.bmp"; echo "shot $n.bmp" > game/sss_cmd.txt
  for i in $(seq 1 40); do [ -f "game/$n.bmp" ] && break; sleep 0.25; done
  sleep 0.3; mkdir -p shots/local
  uv run -q --with pillow python -c "import sys;from PIL import Image;Image.open(sys.argv[1]).save(sys.argv[2])" "game/$n.bmp" "shots/local/$n.png" && rm "game/$n.bmp" && echo "shots/local/$n.png"
  exit
fi
printf '%s\n' "$@" > game/sss_cmd.txt
for i in $(seq 1 20); do [ -f game/sss_cmd.txt ] || break; sleep 0.25; done
tail -n 8 game/sss_mod.log
