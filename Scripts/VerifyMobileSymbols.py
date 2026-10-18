#!/usr/bin/env python3
"""Audit final Android ELF or iPhoneOS Mach-O, not intermediate archives."""
import argparse
import pathlib
import re
import subprocess
import sys

JNI_SYMBOL = "Java_com_moozen_vase_mobile_NativeBridge_runScenario"
JNI_TEST_SYMBOL = "Java_com_moozen_vase_mobile_NativeBridge_runScenarioWithResourceForTest"
JNI_DISABLED_TEST_SYMBOL = "Java_com_moozen_vase_mobile_NativeBridge_runDisabledMemberProbeForTest"


def output(tool: pathlib.Path, *args: str) -> str:
    result = subprocess.run(
        [str(tool), *args], check=False, capture_output=True, text=True, encoding="utf-8"
    )
    if result.returncode != 0:
        raise RuntimeError(f"{tool.name} failed ({result.returncode}): {result.stderr}")
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--elf", type=pathlib.Path)
    target.add_argument("--macho", type=pathlib.Path)
    parser.add_argument("--llvm-nm", type=pathlib.Path, required=True)
    parser.add_argument("--llvm-readobj", type=pathlib.Path)
    parser.add_argument("--llvm-objdump", type=pathlib.Path)
    parser.add_argument("--provider-symbol", default="VasePluginDesc_PackageProviderPlugin")
    parser.add_argument("--consumer-symbol", default="VasePluginDesc_PackageConsumerPlugin")
    parser.add_argument("--forbidden-symbol", default="VasePluginDesc_KeepAliveProbePlugin")
    args = parser.parse_args()
    path = args.elf if args.elf is not None else args.macho
    if not path.is_file():
        raise RuntimeError(f"final binary missing: {path}")

    symbols = output(args.llvm_nm, "--defined-only", str(path))
    names = set()
    for line in symbols.splitlines():
        match = re.match(r"^[0-9a-fA-F]+\s+[A-Za-z]\s+(\S+)$", line)
        if match:
            name = match.group(1)
            # Mach-O's external C symbol names have the standard leading underscore.
            names.add(name[1:] if args.macho is not None and name.startswith("_") else name)

    required_symbols = (
        args.provider_symbol,
        args.consumer_symbol,
        "VasePack_vasepackageprovider_GetPlugin",
        "VasePack_vasepackageprovider_Descriptors",
        "VasePack_vasepackageconsumer_GetPlugin",
        "VasePack_vasepackageconsumer_Descriptors",
    )
    if args.elf is not None:
        required_symbols += (JNI_SYMBOL, JNI_TEST_SYMBOL, JNI_DISABLED_TEST_SYMBOL)
    for required in required_symbols:
        if required not in names:
            raise RuntimeError(f"missing linked symbol: {required}")
    if args.forbidden_symbol in names:
        raise RuntimeError(f"unreferenced plugin unexpectedly linked: {args.forbidden_symbol}")

    if args.elf is not None:
        # Static .symtab presence alone is insufficient for Android's JNI lookup.
        # JNI must also be an externally callable function in ELF .dynsym.
        dynamic = output(args.llvm_nm, "--dynamic", "--defined-only", str(path))
        exported = set()
        for line in dynamic.splitlines():
            match = re.match(r"^[0-9a-fA-F]+\s+([T])\s+(\S+)$", line)
            if match:
                exported.add(match.group(2))
        for symbol in (JNI_SYMBOL, JNI_TEST_SYMBOL, JNI_DISABLED_TEST_SYMBOL):
            if symbol not in exported:
                raise RuntimeError(f"JNI entry not exported in ELF dynamic symbol table: {symbol}")
        if args.llvm_readobj is None:
            raise RuntimeError("--llvm-readobj required for ELF")
        elf = output(args.llvm_readobj, "--file-headers", "--needed-libs", str(path))
        if "Machine: EM_AARCH64" not in elf:
            raise RuntimeError("wrong ELF machine (expected EM_AARCH64)")
        needed = set(re.findall(r"(?m)^\s*(lib[^\s]+\.so)\s*$", elf))
        for library in ("libandroid.so", "libc++_shared.so"):
            if library not in needed:
                raise RuntimeError(f"missing NEEDED library: {library}")
        forbidden_dynamic = {"libVasePod.so", "libVaseHost.so", "libVaseCatalog.so"}
        unexpected = forbidden_dynamic.intersection(needed)
        if unexpected:
            raise RuntimeError(f"static framework unexpectedly depends on shared libraries: {sorted(unexpected)}")
        print("PASS: AArch64, JNI entry, both members, unreferenced probe excluded, shared STL")
    else:
        if args.llvm_objdump is None:
            raise RuntimeError("--llvm-objdump required for Mach-O")
        macho = output(args.llvm_objdump, "--macho", "--private-headers", str(path))
        # A substring check for "platform ios" also accepts "platform iossimulator".
        # Require a single ARM64 Mach header and a single, self-contained iOS
        # LC_BUILD_VERSION instead of mixing fields from different slices/commands.
        headers = re.findall(r"(?m)^MH_MAGIC_64[ \t]+(\S+)[ \t]+", macho)
        if headers != ["ARM64"]:
            raise RuntimeError(f"expected one ARM64 iPhoneOS Mach header, got {headers}")
        versions = re.findall(
            r"(?ms)^[ \t]*cmd LC_BUILD_VERSION[ \t]*\n"
            r"(.*?)(?=^[ \t]*cmd |^MH_MAGIC_64|\Z)",
            macho,
        )
        if len(versions) != 1:
            raise RuntimeError(f"expected one LC_BUILD_VERSION, got {len(versions)}")
        for field, expected in (("platform", "ios"), ("sdk", "18.2"), ("minos", "15.0")):
            values = re.findall(rf"(?m)^[ \t]*{field}[ \t]+(\S+)[ \t]*$", versions[0])
            if values != [expected]:
                raise RuntimeError(f"iPhoneOS Mach-O invalid {field}: {values} (expected {expected})")
        # LC_BUILD_VERSION alone cannot detect unexpected linked dylibs. Audit the
        # final Mach-O's actual load commands, not linker settings or archives.
        dependencies = output(args.llvm_objdump, "--macho", "--dylibs-used", str(path))
        linked = set(re.findall(r"(?m)^\s+(\S+)\s+\(compatibility version", dependencies))
        expected = {
            "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
            "/usr/lib/libc++.1.dylib",
            "/usr/lib/libSystem.B.dylib",
        }
        if linked != expected:
            raise RuntimeError(
                f"unexpected iPhoneOS dylib dependencies: missing={sorted(expected - linked)}, "
                f"extra={sorted(linked - expected)}"
            )
        print("PASS: iPhoneOS arm64 SDK18.2/minOS15.0, device-only dylibs, both static members")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
