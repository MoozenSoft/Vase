@echo off
rem Clean-tree sweep of the four Windows presets: each build tree is deleted, then
rem configure -> build -> ctest -> ctest -N, printing the exit code of every step.
rem Windows twin of Scripts/linux-verify.sh.
rem
rem Usage: Scripts\win-verify.cmd   (runnable from anywhere; it cds to the repo root)
rem Prerequisite: VCPKG_ROOT must already be set. msvc-env.cmd passes it through and
rem an empty one fails loudly at the toolchain guard -- that failure is the point.
rem
rem The -N pass belongs to the gate, not to convenience: ctest exits 0 even when it
rem discovered no tests at all. Read each "Total Tests" against the per-preset
rem baseline table in CLAUDE.md. This script deliberately does not hardcode those
rem numbers -- a second copy of the truth drifts the moment a test is added.
rem
rem Four things below are load-bearing cmd.exe behaviour, not style:
rem
rem 1. ENCODING -- KEEP THIS FILE PURE ASCII. cmd.exe decodes batch with the OEM code
rem    page, not UTF-8, so a non-ASCII comment line can swallow its own CRLF and run
rem    the next line as part of it (Scripts/msvc-env.cmd, quirk 1).
rem
rem 2. `call %*` in :run, not a bare `%*`. The MSVC lines invoke another batch file
rem    (Scripts\msvc-env.cmd); without `call`, cmd does not return to us afterwards
rem    and the sweep stops after one step. For cmake.exe / ctest.exe, `call` is a
rem    no-op, so both shapes go through the same path.
rem
rem 3. Every name this script sets is VASE_-prefixed, and that is not decoration.
rem    `set` in a batch file writes an ENVIRONMENT variable, inherited by every child
rem    process -- unlike bash, where a plain assignment stays private. CMake reads
rem    $ENV{RC} as an RC-compiler override (CMakeDetermineRCCompiler.cmake), so the
rem    first draft's `set "RC=%ERRORLEVEL%"` poisoned every later configure in the
rem    same sweep (observed: the first tree configured fine, the second died with
rem    "Could not find the compiler specified in the environment variable RC: 0.").
rem    Same trap for CC / CXX / AR. `setlocal` scopes the cleanup, not the export.
rem
rem 4. `!VASE_STEP_RC!` rather than `%VASE_STEP_RC%` inside the if-blocks -- the
rem    parser expands `%..%` once per block, before the block runs, so it would read
rem    the previous step's code.

setlocal enabledelayedexpansion
pushd "%~dp0.." || exit /b 1

set "VASE_FAILED="
set "VASE_FAILCOUNT=0"

rem Deleting the trees is not housekeeping. CMAKE_*_FLAGS_INIT only enters the cache
rem on a tree's first configure, so a tree configured before the toolchain files
rem gained /DEBUG:FULL keeps an empty CMAKE_SHARED_LINKER_FLAGS -- no CodeView (RSDS)
rem in the DLLs, and AdoptPlugin then rejects them at runtime.
for %%P in (win-x64-clang-debug win-x64-clang-release win-x64-msvc-debug win-x64-msvc-release) do (
    if exist "build-win\%%P" rmdir /s /q "build-win\%%P"
)

echo ########## win-verify, repo root: %CD% ##########

call :run cmake --preset win-x64-clang-debug
call :run cmake --build --preset win-x64-clang-debug
call :run ctest --preset win-x64-clang-debug
call :run ctest --preset win-x64-clang-debug -N

call :run cmake --preset win-x64-clang-release
call :run cmake --build --preset win-x64-clang-release
call :run ctest --preset win-x64-clang-release
call :run ctest --preset win-x64-clang-release -N

call :run Scripts\msvc-env.cmd cmake --preset win-x64-msvc-debug
call :run Scripts\msvc-env.cmd cmake --build --preset win-x64-msvc-debug
call :run ctest --preset win-x64-msvc-debug
call :run ctest --preset win-x64-msvc-debug -N

call :run Scripts\msvc-env.cmd cmake --preset win-x64-msvc-release
call :run Scripts\msvc-env.cmd cmake --build --preset win-x64-msvc-release
call :run ctest --preset win-x64-msvc-release
call :run ctest --preset win-x64-msvc-release -N

echo.
if "%VASE_FAILCOUNT%"=="0" (
    echo ########## ALL LINES DONE, no failed steps ##########
) else (
    echo ########## ALL LINES DONE, !VASE_FAILCOUNT! failed step[s]: !VASE_FAILED! ##########
)

set "VASE_MAIN_RC=%VASE_FAILCOUNT%"
popd
endlocal & exit /b %VASE_MAIN_RC%

rem One step, echoed the way the Linux twin does: the command, then its exit code.
:run
echo.
echo ########## RUN: %* ##########
call %*
set "VASE_STEP_RC=%ERRORLEVEL%"
echo ########## RC=!VASE_STEP_RC! : %* ##########
if not "!VASE_STEP_RC!"=="0" (
    set /a VASE_FAILCOUNT+=1
    set "VASE_FAILED=!VASE_FAILED! [%* -> RC=!VASE_STEP_RC!]"
)
exit /b %VASE_STEP_RC%
