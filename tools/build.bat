@ECHO OFF
chcp 65001
CLS
setlocal enabledelayedexpansion

@REM
@REM   CHECK THIS PATH IS CORRECT
@REM
CALL "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall" x64

@REM
@REM   DO NOT MODIFY BELOW THIS LINE!
@REM
SET Separator=▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰  ▰
@ECHO.
@ECHO OFF

SET ProgramVersion=_v1_2
SET CommonLibraries=user32.lib Gdi32.lib winmm.lib Gdiplus.lib uxtheme.lib
SET CommonDisableWarnings=-wd4458 -wd4456

if "%~1"=="-debug" goto :BUILD_DEBUG
if "%~1"=="/debug" goto :BUILD_DEBUG
if "%~1"=="-release" goto :BUILD_RELEASE
if "%~1"=="/release" goto :BUILD_RELEASE
if "%~1"=="/-h" goto :HELP
if "%~1"=="-h" goto :HELP
goto :ERROR

:BUILD_DEBUG
IF NOT EXIST ..\build MKDIR ..\build
IF NOT EXIST ..\build\debug MKDIR ..\build\debug
PUSHD ..\build\debug
DEL * /Q > nul 2>&1
SET DebugCompilerFlags=-nologo -std:c++17 -MTd -Gm- -GR- -EHa- -FePcgCamUtility%ProgramVersion% -FdPcgCamUtility%ProgramVersion% -FoPcgCamUtility%ProgramVersion% -Od -Oi -WX -W4 %CommonDisableWarnings% -DPCG_INTERNAL=1 -DPCG_ATTEMPT_VSYNC=1 -FC -Z7 -Fm
@ECHO [95m%Separator%
@ECHO    Building Debug...
@ECHO %Separator%[0m
@ECHO.
SET !ERRORLEVEL!=1
cl %DebugCompilerFlags% ..\..\source\win32_pcg_cam.cpp /link -incremental:no -opt:ref %CommonLibraries%
call ..\..\tools\util\PrintSuccess
POPD
GOTO :END

:BUILD_RELEASE
IF NOT EXIST ..\build MKDIR ..\build
IF NOT EXIST ..\build\release MKDIR ..\build\release
PUSHD ..\build\release
DEL * /Q > nul 2>&1
SET DebugCompilerFlags=-nologo -std:c++17 -EHa- -FePcgCamUtility%ProgramVersion% -O2 -Oi -WX -W4 %CommonDisableWarnings% -DPCG_INTERNAL=0 -DPCG_ATTEMPT_VSYNC=1 -FC
@ECHO [95m%Separator%
@ECHO    Building Release...
@ECHO %Separator%[0m
@ECHO.
SET !ERRORLEVEL!=1
cl %DebugCompilerFlags% ..\..\source\win32_pcg_cam.cpp /link -incremental:no -opt:ref %CommonLibraries%
call ..\..\tools\util\PrintSuccess
POPD
GOTO :END

:HELP
@REM // TODO: Better handling!
@ECHO Usage: Build -[debug/release]
@ECHO.
@ECHO OFF
GOTO :END

:ERROR
@REM // TODO: Better handling!
@ECHO An error occurred. Please ensure you have used the command correctly.
@ECHO OFF
GOTO :END

:END