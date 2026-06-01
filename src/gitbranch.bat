@echo off
REM Regenerate BuildId.h on every build.  Defines:
REM   GIT_BRANCH       current branch (or "master" if git is unavailable)
REM   CONCAD_BUILD     monotonically increasing commit count on the current
REM                    branch (0 if git is unavailable)
REM   BUILD_UUID       a fresh per-build UUID
REM
REM MSBuild's pre-build event runs cmd with the system PATH, which often
REM lacks the per-user Git-for-Windows install.  Look in a few common
REM places before giving up.

setlocal enabledelayedexpansion

set "GIT_EXE="
where git >nul 2>nul
if %ERRORLEVEL% EQU 0 set "GIT_EXE=git"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe"        set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\bin\git.exe"        set "GIT_EXE=%ProgramFiles%\Git\bin\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles(x86)%\Git\cmd\git.exe"   set "GIT_EXE=%ProgramFiles(x86)%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles(x86)%\Git\bin\git.exe"   set "GIT_EXE=%ProgramFiles(x86)%\Git\bin\git.exe"
if not defined GIT_EXE if exist "%LocalAppData%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LocalAppData%\Programs\Git\cmd\git.exe"

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
