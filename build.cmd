@echo off
setlocal enabledelayedexpansion

if not defined VS100COMNTOOLS (
    echo [ERROR] VS100COMNTOOLS not set. Please run this from a VS2010 command prompt.
    exit /b 1
)

set VCVARS="%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat"

set CLI_SRC=main_cli.cpp common.cpp
set GUI_SRC=main_gui.cpp common.cpp
set RES=app.rc

set CLI_LIBS=bcrypt.lib
set GUI_LIBS=bcrypt.lib user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib

REM ============================================================
REM  编译 x86
REM ============================================================
echo.
echo ############################################
echo #  building x86
echo ############################################
call %VCVARS% x86
if errorlevel 1 goto :fail

echo.
echo === [res] app.rc ===
rc /nologo /fo app.res app.rc
if errorlevel 1 goto :fail

echo.
echo === [x86] CLI ===
cl /nologo /O2 /GL /EHsc /MT /DUNICODE /D_UNICODE %CLI_SRC% app.res ^
   /link /LTCG /OPT:REF /OPT:ICF %CLI_LIBS% /SUBSYSTEM:CONSOLE /OUT:LenovoEduCloudPWSet.exe
if errorlevel 1 goto :fail
del /Q *.obj >nul 2>nul

echo.
echo === [x86] GUI ===
cl /nologo /O2 /GL /EHsc /MT /DUNICODE /D_UNICODE %GUI_SRC% app.res ^
   /link /LTCG /OPT:REF /OPT:ICF %GUI_LIBS% /SUBSYSTEM:WINDOWS /OUT:LenovoEduCloudPWSet_GUI.exe
if errorlevel 1 goto :fail
del /Q *.obj >nul 2>nul

REM ============================================================
REM  编译 x64
REM ============================================================
echo.
echo ############################################
echo #  building x64
echo ############################################
call %VCVARS% amd64
if errorlevel 1 goto :fail

echo.
echo === [res] app.rc (x64) ===
rc /nologo /fo app.res app.rc
if errorlevel 1 goto :fail

echo.
echo === [x64] CLI ===
cl /nologo /O2 /GL /EHsc /MT /DUNICODE /D_UNICODE %CLI_SRC% app.res ^
   /link /LTCG /OPT:REF /OPT:ICF %CLI_LIBS% /SUBSYSTEM:CONSOLE /OUT:LenovoEduCloudPWSet64.exe
if errorlevel 1 goto :fail
del /Q *.obj >nul 2>nul

echo.
echo === [x64] GUI ===
cl /nologo /O2 /GL /EHsc /MT /DUNICODE /D_UNICODE %GUI_SRC% app.res ^
   /link /LTCG /OPT:REF /OPT:ICF %GUI_LIBS% /SUBSYSTEM:WINDOWS /OUT:LenovoEduCloudPWSet_GUI64.exe
if errorlevel 1 goto :fail
del /Q *.obj >nul 2>nul

del /Q app.res >nul 2>nul

REM ============================================================
REM  完成
REM ============================================================
echo.
echo ========================================
echo  build ok
echo ========================================
echo   x86 CLI: %~dp0LenovoEduCloudPWSet.exe
echo   x86 GUI: %~dp0LenovoEduCloudPWSet_GUI.exe
echo   x64 CLI: %~dp0LenovoEduCloudPWSet64.exe
echo   x64 GUI: %~dp0LenovoEduCloudPWSet_GUI64.exe
echo ========================================
echo.
goto :eof

:fail
echo.
echo === build failed ===
exit /b 1