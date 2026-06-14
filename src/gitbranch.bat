@echo off
REM Regenerate BuildId.h on every build.  Defines:
REM   GIT_BRANCH       current branch (or "master" if git is unavailable)
REM   CONCAD_BUILD     monotonically increasing commit count on the current
REM                    branch (0 if git is unavailable)
REM   BUILD_UUID       a fresh per-build UUID
REM
REM MSBuild's pre-build event runs cmd with the system PATH, which often
REM lacks the per-user Git-for-Windows install.  Look in a few common
REM places before giving up - including the git.exe that ships with
REM Visual Studio (via the VSINSTALLDIR / DevEnvDir build environment
REM variables), so no separate Git-for-Windows install is required.

setlocal enabledelayedexpansion

set "GIT_EXE="
where git >nul 2>nul
if %ERRORLEVEL% EQU 0 set "GIT_EXE=git"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe"        set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\bin\git.exe"        set "GIT_EXE=%ProgramFiles%\Git\bin\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles(x86)%\Git\cmd\git.exe"   set "GIT_EXE=%ProgramFiles(x86)%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles(x86)%\Git\bin\git.exe"   set "GIT_EXE=%ProgramFiles(x86)%\Git\bin\git.exe"
if not defined GIT_EXE if exist "%LocalAppData%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LocalAppData%\Programs\Git\cmd\git.exe"
REM git.exe bundled with Visual Studio (Team Explorer).
set "VS_GIT=Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
REM First try the VS build-environment variables (set when building from a
REM developer prompt); these end with a "\".
if not defined GIT_EXE if defined VSINSTALLDIR if exist "%VSINSTALLDIR%%VS_GIT%" set "GIT_EXE=%VSINSTALLDIR%%VS_GIT%"
if not defined GIT_EXE if defined DevEnvDir if exist "%DevEnvDir%CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" set "GIT_EXE=%DevEnvDir%CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
REM Most reliable: ask vswhere (fixed location) for the VS install path.
REM Use delayed expansion (!VSWHERE!) so the "(x86)" parens in the path do
REM not break cmd's parenthesis matching inside the for/f block.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined GIT_EXE if exist "!VSWHERE!" for /f "usebackq delims=" %%a in (`""!VSWHERE!" -latest -property installationPath" 2^>nul`) do if exist "%%a\!VS_GIT!" set "GIT_EXE=%%a\!VS_GIT!"

set "BRANCH=master"
set "BUILD=0"
if defined GIT_EXE (
    for /f "delims=" %%a in ('""%GIT_EXE%" rev-parse --abbrev-ref HEAD" 2^>nul') do set "BRANCH=%%a"
    for /f "delims=" %%a in ('""%GIT_EXE%" rev-list --count HEAD" 2^>nul')        do set "BUILD=%%a"
)

> BuildId.h echo #define GIT_BRANCH "!BRANCH!"
>> BuildId.h echo #define CONCAD_BUILD !BUILD!
for /f %%a in ('"uuidgen"') do echo #define BUILD_UUID "%%a" >> BuildId.h

endlocal
