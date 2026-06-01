@echo off
REM Regenerate BuildId.h on every build.  Defines:
REM   GIT_BRANCH       current branch (or "master" if git is unavailable)
REM   CONCAD_BUILD     monotonically increasing commit count on the current
REM                    branch (0 if git is unavailable)
REM   BUILD_UUID       a fresh per-build UUID

where git >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    for /f %%a in ('"git rev-parse --abbrev-ref HEAD"') do echo #define GIT_BRANCH "%%a" > BuildId.h
    for /f %%a in ('"git rev-list --count HEAD"') do echo #define CONCAD_BUILD %%a >> BuildId.h
) else (
    echo #define GIT_BRANCH "master" > BuildId.h
    echo #define CONCAD_BUILD 0 >> BuildId.h
)
for /f %%a in ('"uuidgen"') do echo #define BUILD_UUID "%%a" >> BuildId.h
