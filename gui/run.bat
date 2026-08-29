@echo off
setlocal
cd /d "%~dp0"

set BIN=..\third_party\neutralinojs\neutralino-win_x64.exe
set CLIB=..\third_party\neutralinojs\neutralino.js
if not exist "%BIN%" (
  echo [!] 未找到 Neutralino 运行时: %BIN%
  echo     请先下载 neutralinojs-win_x64 二进制到 third_party\neutralinojs\
  pause
  exit /b 1
)
if not exist "%CLIB%" (
  echo [!] 未找到 Neutralino 客户端库: %CLIB%
  pause
  exit /b 1
)

copy /y "%BIN%" neutralino-win_x64.exe >nul
if not exist resources\js mkdir resources\js
copy /y "%CLIB%" resources\js\neutralino.js >nul

echo [*] 启动 N0va 壁纸助手 GUI ...
neutralino-win_x64.exe --load-dir-res

del neutralino-win_x64.exe >nul 2>&1
rmdir /s /q resources\js >nul 2>&1

