@echo off
rem Usage: Scripts\msvc-env.cmd <command...>
rem
rem Why this script exists: cl.exe depends on PATH / INCLUDE / LIB being set by
rem vcvars64.bat, while clang-cl does not (it discovers MSVC through the
rem registry). This script funnels vcvars into one entry point, so the MSVC
rem build line is as simple to use as the clang-cl one.
rem
rem vswhere is used to locate VS instead of hardcoding a versioned path --
rem the same principle as the toolchain file not hardcoding an LLVM path.
rem
rem Two cmd.exe quirks are worked around below. Both were reproduced on this
rem machine, and the symptoms are quoted in the comments themselves so they can
rem be re-checked without any external document.
rem
rem 1. ENCODING -- KEEP THIS FILE PURE ASCII.
rem    cmd.exe decodes batch files with the OEM code page, not UTF-8. A
rem    non-ASCII line whose last byte happens to look like a lead byte in that
rem    code page swallows the following CRLF, merging it with the next line;
rem    the parser then runs the merged text as a command. A UTF-8 BOM is worse
rem    (even "@echo off" fails), and `chcp 65001` does not help because cmd has
rem    already buffered the file.
rem
rem 2. DELAYED EXPANSION -- parentheses inside expanded paths.
rem    %ProgramFiles(x86)% itself is fine, but once VSWHERE holds a path
rem    containing "(x86)", writing `if not exist "%VSWHERE%" (` breaks: the
rem    parser scans for the block-opening "(" before expanding the variable and
rem    treats the ")" in "(x86)" as closing it, yielding
rem    "\Microsoft was unexpected at this time." Quotes do not protect it.
rem    Expanding with !VSWHERE! defers substitution to execution time, so the
rem    parser only ever sees the paren-free literal.

setlocal enabledelayedexpansion

rem vcvars64.bat overrides VCPKG_ROOT with the vcpkg bundled inside Visual
rem Studio (observed: VS 18 sets it to ...\VC\vcpkg). Vase pins one specific
rem vcpkg -- see the builtin-baseline in vcpkg.json -- so the caller's value is
rem what must survive. Remember it now, then apply it again after vcvars.
set "VASE_VCPKG_ROOT=%VCPKG_ROOT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [msvc-env] vswhere.exe not found: !VSWHERE!
    echo [msvc-env] Install the Visual Studio C++ build tools.
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if "!VSINSTALL!"=="" (
    echo [msvc-env] vswhere found no VS install with the C++ tools.
    exit /b 1
)

call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [msvc-env] vcvars64.bat failed.
    exit /b 1
)

rem Restore the caller's VCPKG_ROOT. When the caller had none, CLEAR the value
rem vcvars just installed instead of keeping it: a non-empty-but-wrong
rem VCPKG_ROOT passes the toolchain file's "is it set?" guard, so leaving VS's
rem bundled vcpkg in place would silently configure the MSVC preset against a
rem non-pinned vcpkg (observed: it then tries to fetch the registry over the
rem network and hangs). Clearing makes that case fail loudly at the guard,
rem which is what we want on a machine or CI job that never set VCPKG_ROOT.
if "!VASE_VCPKG_ROOT!"=="" (set "VCPKG_ROOT=") else (set "VCPKG_ROOT=!VASE_VCPKG_ROOT!")

%*
exit /b %ERRORLEVEL%
