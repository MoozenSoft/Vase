#!/usr/bin/env python3
"""Regression witness: ELF JNI entry must be dynamically exported, not just defined."""
import importlib.util
import pathlib
import sys
import unittest
from unittest import mock

VERIFIER = pathlib.Path(__file__).resolve().parents[2] / "Scripts" / "VerifyMobileSymbols.py"
SPEC = importlib.util.spec_from_file_location("verify_mobile_symbols_under_test", VERIFIER)
# Do not write Python bytecode into the source tree while importing the verifier.
sys.dont_write_bytecode = True
verifier = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verifier)

BASE_SYMBOLS = (
    "VasePluginDesc_PackageProviderPlugin",
    "VasePluginDesc_PackageConsumerPlugin",
    "VasePack_vasepackageprovider_GetPlugin",
    "VasePack_vasepackageprovider_Descriptors",
    "VasePack_vasepackageconsumer_GetPlugin",
    "VasePack_vasepackageconsumer_Descriptors",
    verifier.JNI_SYMBOL,
    verifier.JNI_TEST_SYMBOL,
    verifier.JNI_DISABLED_TEST_SYMBOL,
)


class MobileSymbolsTests(unittest.TestCase):
    def run_verifier(self, exported_type, test_entry_exported=True):
        normal = "".join(f"{i + 1:016x} T {name}\n" for i, name in enumerate(BASE_SYMBOLS))
        dynamic = (
            f"00000000000000ab {exported_type} {verifier.JNI_SYMBOL}\n"
            if exported_type is not None else ""
        )
        if test_entry_exported:
            dynamic += f"00000000000000cd T {verifier.JNI_TEST_SYMBOL}\n"
        dynamic += f"00000000000000de T {verifier.JNI_DISABLED_TEST_SYMBOL}\n"
        headers = "Machine: EM_AARCH64\nNeededLibraries [\n  libandroid.so\n  libc++_shared.so\n]\n"

        def fake_output(tool, *args):
            if "--dynamic" in args:
                return dynamic
            if "--file-headers" in args:
                return headers
            return normal

        arguments = [
            "VerifyMobileSymbols.py",
            "--elf",
            str(VERIFIER),
            "--llvm-nm",
            str(VERIFIER),
            "--llvm-readobj",
            str(VERIFIER),
        ]
        with mock.patch.object(verifier, "output", side_effect=fake_output):
            with mock.patch.object(sys, "argv", arguments):
                return verifier.main()

    def test_exported_jni_function_passes(self):
        self.assertEqual(self.run_verifier("T"), 0)

    def test_symbol_only_in_normal_table_fails(self):
        with self.assertRaisesRegex(RuntimeError, "not exported in ELF dynamic symbol table"):
            self.run_verifier(None)

    def test_non_function_dynamic_symbol_fails(self):
        with self.assertRaisesRegex(RuntimeError, "not exported in ELF dynamic symbol table"):
            self.run_verifier("D")

    def test_instrumentation_entry_missing_from_dynamic_table_fails(self):
        with self.assertRaisesRegex(RuntimeError, "not exported in ELF dynamic symbol table"):
            self.run_verifier("T", test_entry_exported=False)


    def run_macho(
        self, linked_libraries, *, platform="ios", sdk="18.2", minos="15.0",
        architectures=("ARM64",),
    ):
        normal = "".join(
            f"{i + 1:016x} T _{name}\n" for i, name in enumerate(BASE_SYMBOLS)
        )
        headers = "".join(
            f"Mach header\nMH_MAGIC_64   {architecture}        ALL\n"
            f"       cmd LC_BUILD_VERSION\n"
            f"  platform {platform}\n       sdk {sdk}\n     minos {minos}\n"
            for architecture in architectures
        )
        dependencies = "app:\n" + "".join(
            f"    {library} (compatibility version 1.0.0, current version 1.0.0)\n"
            for library in linked_libraries
        )

        def fake_output(tool, *args):
            if "--private-headers" in args:
                return headers
            if "--dylibs-used" in args:
                return dependencies
            return normal

        arguments = [
            "VerifyMobileSymbols.py",
            "--macho",
            str(VERIFIER),
            "--llvm-nm",
            str(VERIFIER),
            "--llvm-objdump",
            str(VERIFIER),
        ]
        with mock.patch.object(verifier, "output", side_effect=fake_output):
            with mock.patch.object(sys, "argv", arguments):
                return verifier.main()

    def test_ios_sdk_only_dylibs_pass(self):
        self.assertEqual(
            self.run_macho(
                [
                    "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
                    "/usr/lib/libc++.1.dylib",
                    "/usr/lib/libSystem.B.dylib",
                ]
            ),
            0,
        )

    def test_ios_rejects_macports_dylib(self):
        with self.assertRaisesRegex(RuntimeError, "unexpected iPhoneOS dylib dependencies"):
            self.run_macho(
                [
                    "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
                    "/usr/lib/libc++.1.dylib",
                    "/usr/lib/libSystem.B.dylib",
                    "/opt/local/lib/libc++.1.dylib",
                ]
            )

    def test_ios_rejects_rpath_dependency(self):
        with self.assertRaisesRegex(RuntimeError, "unexpected iPhoneOS dylib dependencies"):
            self.run_macho(
                [
                    "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
                    "/usr/lib/libc++.1.dylib",
                    "/usr/lib/libSystem.B.dylib",
                    "@rpath/libUnexpected.dylib",
                ]
            )

    def test_ios_rejects_missing_sdk_dependency(self):
        with self.assertRaisesRegex(RuntimeError, "unexpected iPhoneOS dylib dependencies"):
            self.run_macho(["/usr/lib/libSystem.B.dylib"])

    def test_ios_rejects_arm64_simulator_even_with_correct_dylibs(self):
        with self.assertRaisesRegex(RuntimeError, "invalid platform"):
            self.run_macho([], platform="iossimulator")

    def test_ios_rejects_macos_platform(self):
        with self.assertRaisesRegex(RuntimeError, "invalid platform"):
            self.run_macho([], platform="macos")

    def test_ios_rejects_different_sdk(self):
        with self.assertRaisesRegex(RuntimeError, "invalid sdk"):
            self.run_macho([], sdk="18.1")

    def test_ios_rejects_different_minimum_os(self):
        with self.assertRaisesRegex(RuntimeError, "invalid minos"):
            self.run_macho([], minos="14.0")

    def test_ios_rejects_arm64e_architecture(self):
        with self.assertRaisesRegex(RuntimeError, "expected one ARM64"):
            self.run_macho([], architectures=("ARM64E",))

    def test_ios_rejects_multi_slice_headers(self):
        with self.assertRaisesRegex(RuntimeError, "expected one ARM64"):
            self.run_macho([], architectures=("ARM64", "ARM64"))


if __name__ == "__main__":
    unittest.main()
