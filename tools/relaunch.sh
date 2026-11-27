#!/usr/bin/env bash
# 重建 DLL → 部署到 game/ → 重開 game/WUJIN.exe -window(只動複本)
cd "$(dirname "$0")/.." || exit 1
taskkill //IM WUJIN.exe //F >/dev/null 2>&1; sleep 1
out=$(cmd //c "mod\build.cmd" 2>&1 | iconv -f cp950 -t utf-8 | grep -E "error|BUILD OK")
echo "$out"; echo "$out" | grep -q "BUILD OK" || { echo "BUILD FAILED" >&2; exit 1; }   # 有 error 就停,不要拿舊 DLL 繼續跑
cp mod/build/dsound.dll game/dsound.dll
(cd game && powershell -NoProfile -Command "Start-Process -FilePath '.\WUJIN.exe' -ArgumentList '-window' -WorkingDirectory '.'")
sleep ${1:-12}
