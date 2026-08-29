@echo off
setlocal
cd /d "%~dp0"

rem Uses the official neu CLI to bundle (hand-rolled ASAR is error-prone).
rem Requires: Node.js + npx, internet for the first npx fetch.
rem Output: dist\n0va-wallpaper-gui\  (exe + resources.neu must stay together)

npx --yes @neutralinojs/neu build --release
if errorlevel 1 exit /b 1

cd /d "%~dp0dist\n0va-wallpaper-gui"
copy /y n0va-wallpaper-gui-win_x64.exe n0va-wallpaper-gui.exe >nul

echo.
echo [OK] Packaged: dist\n0va-wallpaper-gui\n0va-wallpaper-gui.exe
echo      Keep the exe and resources.neu in the same folder.
echo      n0va_plugin.exe must be in the same folder as the GUI exe.
endlocal
