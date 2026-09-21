@echo off
rem clang-tidy gate for the two Windows debug lines -- Windows twin of
rem Scripts/linux-clang-tidy.sh: same three judgments, same summary numbers, same
rem "exit code = verdict" contract.
rem
rem Usage: Scripts\win-clang-tidy.cmd            both lines (clangcl then msvc)
rem        Scripts\win-clang-tidy.cmd clangcl    one line only: clangcl | msvc
rem
rem Per-line logs land next to this script: Scripts\win-clang-tidy-clangcl.log and
rem Scripts\win-clang-tidy-msvc.log. They are *.log, so .gitignore already covers
rem them -- they are not files someone forgot to add.
rem
rem Exit code: number of lines that did not pass (0 = green; 2 = could not start,
rem e.g. bad argument or missing tools). All three judgments must hold, because
rem run-clang-tidy returns 0 even when the log is full of warnings -- .clang-tidy
rem leaves WarningsAsErrors empty.
rem
rem The baseline numbers (48 files, Suppressed sum 309464, NOLINT hits 30 per line)
rem live in CLAUDE.md's table and are deliberately NOT duplicated here as thresholds
rem -- a second copy of the truth drifts. This script prints them, you compare.
rem
rem Five cmd.exe facts are load-bearing below, not style:
rem
rem 1. run-clang-tidy cannot be invoked by name. LLVM installs it as an EXTENSIONLESS
rem    Python script (observed here: D:\Developer\LLVM\bin\run-clang-tidy), and cmd
rem    only execs names whose extension is in PATHEXT -- from a batch file it dies with
rem    "not recognized", while Git Bash runs it happily through the shebang. Hence:
rem    resolve it with where.exe, then run `python <that>`. The path is not hardcoded,
rem    same reason the toolchain files do not hardcode an LLVM path.
rem
rem 2. find must be %SystemRoot%\System32\find.exe, spelled out. Launched from Git
rem    Bash (this repo's documented shell), a bare `find` resolves to Git's GNU find,
rem    which reads /c as a starting directory and walks the whole C: tree instead of
rem    counting your log (observed: 120 s and climbing before it was killed).
rem
rem 3. Inside for /f ('...'), that exe path stays UNQUOTED -- quoting it collapses cmd's
rem    nested-quote parsing and the arguments come apart (observed: the error line came
rem    back as `...System32\find.exe" "Suppressed` is not recognized). C:\WINDOWS
rem    \System32 has no spaces. VASE_RCT may have some, so it IS quoted everywhere it
rem    appears as an ordinary argument to python.
rem
rem 4. VASE_ on every name this script sets, and !VAR! inside blocks. `set` in a batch
rem    file writes an ENVIRONMENT variable inherited by children; CMake reads $ENV{RC},
rem    CC and CXX, so one unqualified RC poisons every later configure (see
rem    Scripts/win-verify.cmd quirk 3 -- this was hit once already).
rem
rem 5. cmd also splits CALL arguments on `=`, so the cl.exe line's flag must arrive
rem    QUOTED and be read back with %~3. Unquoted, %3 was just "-extra-arg" and the
rem    value fell into %4 -- observed as run-clang-tidy failing with "argument
rem    -extra-arg: expected one argument". The gate caught its own wrapper bug that
rem    way, which is the argument for failing closed on any body "error:" line.
rem
rem Output is ASCII-only for the same reason the comments are: cmd decodes batch with
rem the OEM code page, and prints what it decoded.

setlocal enabledelayedexpansion
pushd "%~dp0.." || exit /b 1

set "VASE_FIND=%SystemRoot%\System32\find.exe"
set "VASE_WHO=%~1"
set "VASE_FAILCOUNT=0"
set "VASE_RUNCOUNT=0"

set "VASE_RCT="
for /f "delims=" %%I in ('%SystemRoot%\System32\where.exe run-clang-tidy 2^>nul') do if not defined VASE_RCT set "VASE_RCT=%%I"
if not defined VASE_RCT (
    echo [win-clang-tidy] run-clang-tidy not on PATH -- LLVM 23.x required, see CLAUDE.md.
    set "VASE_FAILCOUNT=2"
    goto :finish
)
python -V >nul 2>&1
if errorlevel 1 (
    echo [win-clang-tidy] no usable python on PATH; run-clang-tidy is an extensionless Python script.
    set "VASE_FAILCOUNT=2"
    goto :finish
)

if "%VASE_WHO%"=="" (
    call :line clangcl win-x64-clang-debug
    call :line msvc win-x64-msvc-debug "-extra-arg=-Wno-unused-command-line-argument"
) else if "%VASE_WHO%"=="clangcl" (
    call :line clangcl win-x64-clang-debug
) else if "%VASE_WHO%"=="msvc" (
    call :line msvc win-x64-msvc-debug "-extra-arg=-Wno-unused-command-line-argument"
) else (
    echo [win-clang-tidy] unknown line "%VASE_WHO%" -- expected clangcl, msvc, or no argument.
    set "VASE_FAILCOUNT=2"
    goto :finish
)

:finish
echo.
if not "%VASE_FAILCOUNT%"=="0" goto :report_fail
if "%VASE_RUNCOUNT%"=="0" goto :report_fail
echo ########## TIDY GATE PASS, %VASE_RUNCOUNT% line[s] green ##########
popd
endlocal & exit /b 0

:report_fail
echo ########## TIDY GATE NOT GREEN, %VASE_FAILCOUNT% failed line[s], %VASE_RUNCOUNT% line[s] attempted ##########
popd
endlocal & exit /b %VASE_FAILCOUNT%

rem One tidy line. %~1 = tag (names the log), %~2 = preset, %3 = extra arg for
rem run-clang-tidy, if any. The -extra-arg goes to the cl.exe line only: adding it to
rem clang-cl would hide a diagnostic that line does not currently produce.
:line
set "VASE_TAG=%~1"
set "VASE_PRESET=%~2"
set "VASE_EXTRA=%~3"
set "VASE_LOG=%~dp0win-clang-tidy-%~1.log"
set /a VASE_RUNCOUNT+=1
echo.
echo ########## %VASE_TAG%: build-win\%VASE_PRESET% ##########

if not exist "build-win\%VASE_PRESET%\compile_commands.json" (
    echo [win-clang-tidy] missing build-win\%VASE_PRESET%\compile_commands.json -- run: cmake --preset %VASE_PRESET%
    set /a VASE_FAILCOUNT+=1
    exit /b 1
)

python "%VASE_RCT%" -p "build-win\%VASE_PRESET%" %VASE_EXTRA% > "%VASE_LOG%" 2>&1
set "VASE_STEP_RC=%ERRORLEVEL%"

call :count ": error: " VASE_ERR
call :count ": warning: " VASE_WARN
rem The Linux twin counts "any file:line:col shaped body line" with a regex. cmd has no
rem usable regex for it (findstr lacks + and alternation), so the two other diagnostic
rem kinds clang-tidy emits are named instead. Reference only, not part of the gate.
call :count ": note: " VASE_NOTE
call :count ": remark: " VASE_REMARK
call :count "warnings generated" VASE_GENROWS
call :count "Suppressed " VASE_SUPPROWS

set /a VASE_SUPPSUM=0
for /f "tokens=2" %%N in ('%VASE_FIND% "Suppressed " ^< "%VASE_LOG%"') do set /a VASE_SUPPSUM+=%%N
set /a VASE_NOLINTSUM=0
for /f "tokens=3 delims=,()" %%K in ('%VASE_FIND% "NOLINT" ^< "%VASE_LOG%"') do for /f "tokens=1" %%M in ("%%K") do set /a VASE_NOLINTSUM+=%%M
rem The two row counts reported per line are NOT equal by design: the compilation
rem database lists 49 entries, but fixtures\LoadProbe\LoadProbe.cpp is built into both
rem the LoadProbe and the UnloadProbe fixture target, and run-clang-tidy dedupes by
rem path (its own "48 files out of 48" is the deduped count too). Same on Linux --
rem see Scripts/linux-clang-tidy.sh.

set "VASE_FC="
for /f "tokens=*" %%L in ('%VASE_FIND% "files out of" ^< "%VASE_LOG%"') do if not defined VASE_FC set "VASE_FC=%%L"

echo log=%VASE_LOG%
echo 1. run-clang-tidy exit code = %VASE_STEP_RC%
echo 2. body error lines   = %VASE_ERR%
echo 3. body warning lines = %VASE_WARN%
echo    reference only: body note = %VASE_NOTE%, body remark = %VASE_REMARK%
if defined VASE_FC (echo    %VASE_FC%) else (echo    no "files out of" line -- tidy did not reach the compilation database)
echo    summary rows: warnings generated = %VASE_GENROWS%, Suppressed = %VASE_SUPPROWS%
echo    sums: Suppressed = %VASE_SUPPSUM%, NOLINT hits = %VASE_NOLINTSUM%

set "VASE_LINE_BAD="
if not "%VASE_STEP_RC%"=="0" set "VASE_LINE_BAD=1"
if not "%VASE_ERR%"=="0" set "VASE_LINE_BAD=1"
if not "%VASE_WARN%"=="0" set "VASE_LINE_BAD=1"
if defined VASE_LINE_BAD (
    set /a VASE_FAILCOUNT+=1
    echo ---------- %VASE_TAG%: not green, body diagnostics follow ----------
    %VASE_FIND% ": error: " < "%VASE_LOG%"
    %VASE_FIND% ": warning: " < "%VASE_LOG%"
) else (
    echo ---------- %VASE_TAG%: green ----------
)
exit /b 0

rem Count lines containing a literal string in the current line's log.
rem %1 arrives WITH its quotes -- that is what find wants for strings holding spaces.
rem %~2 is the name of the variable that receives the count.
:count
set "%~2=0"
for /f "tokens=*" %%N in ('%VASE_FIND% /c %1 ^< "%VASE_LOG%"') do set "%~2=%%N"
exit /b 0
