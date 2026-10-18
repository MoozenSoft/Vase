"""Regression tests for Android instrumentation acceptance; no device needed."""
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Scripts" / "VerifyAndroidInstrumentation.py"
spec = importlib.util.spec_from_file_location("verify_android_instrumentation", SCRIPT)
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


def trace(test_count=8, endings=None, runner_end=True, duplicate=False):
    lines = []
    for i in range(test_count):
        name = f"test{i if not duplicate or i != 1 else 0}"
        lines += [
            "INSTRUMENTATION_STATUS: class=com.moozen.vase.mobile.MobilePackageTest",
            f"INSTRUMENTATION_STATUS: current={i + 1}",
            f"INSTRUMENTATION_STATUS: numtests={test_count}",
            f"INSTRUMENTATION_STATUS: test={name}",
            "INSTRUMENTATION_STATUS_CODE: 1",
            f"INSTRUMENTATION_STATUS: test={name}",
            f"INSTRUMENTATION_STATUS_CODE: {(endings or {}).get(i, 0)}",
        ]
    lines += [f"OK ({test_count} tests)"]
    if runner_end:
        lines += ["INSTRUMENTATION_CODE: -1"]
    return "\n".join(lines) + "\n"


def report(tests="8", failures="0", skipped="0"):
    return f"""<html><div id="summary">
    <div class="infoBox" id="tests"><div class="counter">{tests}</div></div>
    <div class="infoBox" id="failures"><div class="counter">{failures}</div></div>
    <div class="infoBox" id="skipped"><div class="counter">{skipped}</div></div>
    </div></html>"""


class AndroidAcceptanceTests(unittest.TestCase):
    def test_eight_unique_passes_and_runner_finished(self):
        verifier.validate_adb_trace(trace(), 8)

    def test_zero_tests_rejected_even_if_gradle_exits_zero(self):
        with self.assertRaises(ValueError):
            verifier.validate_adb_trace("OK (0 tests)\nINSTRUMENTATION_CODE: -1\n", 8)

    def test_failure_status_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_adb_trace(trace(endings={4: -2}), 8)

    def test_missing_terminal_code_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_adb_trace(trace(runner_end=False), 8)

    def test_duplicate_test_name_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_adb_trace(trace(duplicate=True), 8)

    def test_mismatched_declared_total_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_adb_trace(trace(test_count=7), 8)

    def test_gradle_zero_tests_report_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_gradle_report(report(tests="0"), 8)

    def test_gradle_failure_or_skip_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_gradle_report(report(failures="1"), 8)
        with self.assertRaises(ValueError):
            verifier.validate_gradle_report(report(skipped="1"), 8)

    def test_gradle_eight_tests_accepted(self):
        verifier.validate_gradle_report(report(), 8)

    def test_missing_gradle_report_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises((OSError, ValueError)):
                verifier.check_gradle_report(pathlib.Path(d) / "index.html", 8)

    def test_real_device_trace(self):
        evidence = ROOT / ".superpowers" / "sdd" / "2026-10-16-vasepack-mobile" / "android-device-20261018-manual-install-instrumentation.log"
        if evidence.exists():
            verifier.validate_adb_trace(evidence.read_text(encoding="utf-8-sig"), 8)


if __name__ == "__main__":
    unittest.main()
