@echo off
REM Write BuildId.h with the current branch name and a fresh build UUID.
REM If git is not available, fall back to "master" so the build still works.

where git >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    for /f %%a in ('"git rev-parse --abbrev-ref HEAD"') do echo #define GIT_BRANCH "%%a" > BuildId.h
) else (
    echo #define GIT_BRANCH "master" > BuildId.h
)
for /f %%a in ('"uuidgen"') do echo #define BUILD_UUID "%%a" >> BuildId.h
