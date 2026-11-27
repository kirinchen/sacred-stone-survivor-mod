@echo off
rem 用 VS2022 BuildTools 的 32-bit MSVC 建 dsound.dll 代理(遊戲是 32-bit)。產物在 mod\build\(gitignore)。
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0build"
cl /nologo /utf-8 /O2 /W3 /LD /MT ..\src\sssmod.c /Fe:dsound.dll user32.lib gdi32.lib || exit /b 1
echo BUILD OK
