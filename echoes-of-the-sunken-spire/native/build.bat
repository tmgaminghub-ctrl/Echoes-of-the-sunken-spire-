@echo off
REM ---------------------------------------------------------------------------
REM  KayKit Native - Windows build with Visual Studio (MSVC)
REM  Run this from an "x64 Native Tools Command Prompt for VS".
REM
REM    build.bat              -> spire.exe  (with icon + version info)
REM
REM  This is the *development* build: it reads assets from a folder on disk
REM  (.\assets, or --data <dir>). For the single-file portable release, where
REM  the assets are baked into the executable, use build_exe.sh on a Linux/WSL
REM  box (mingw-w64) which produces the .exe shipped in the release zip.
REM ---------------------------------------------------------------------------
setlocal
if not exist build mkdir build

REM resources: taskbar icon, version block, DPI manifest
rc /nologo /I win /fo build\app.res win\spire.rc
if errorlevel 1 goto fail

cl /nologo /std:c++17 /O2 /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS ^
   /Fo:build\ /Fe:spire.exe ^
   src\gltf.cpp src\render.cpp src\platform.cpp ^
   src\game.cpp src\classes.cpp src\worlds.cpp src\main.cpp ^
   build\app.res ^
   /link user32.lib gdi32.lib xinput.lib ^
   /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup

if errorlevel 1 goto fail
echo.
echo Built spire.exe   (run it with an assets folder beside it, e.g.:
echo   spire.exe --game hollowmere --data ..\game\public\assets )
endlocal
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1
