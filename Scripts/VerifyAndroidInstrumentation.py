#!/usr/bin/env python3
"""Strict acceptance of Gradle connected reports and AndroidJUnitRunner receipts."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


def validate_gradle_report(html: str, expected: int) -> None:
    """Reject missing, zero-test, partial, failed or skipped Gradle summaries."""
    numbers: dict[str, int] = {}
    for name in ("tests", "failures", "skipped"):
        match = re.search(
            r'<div\s+class="infoBox"\s+id="' + name
            + r'">\s*<div\s+class="counter">([0-9]+)</div>',
            html,
        )
        if not match:
            raise ValueError(f"Gradle report missing {name} counter")
        numbers[name] = int(match.group(1))
    if numbers != {"tests": expected, "failures": 0, "skipped": 0}:
        raise ValueError(f"Gradle tests are not all green: {numbers}, expected {expected} tests")
    print(f"PASS: Gradle connected report {expected} tests, 0 failures, 0 skipped")


def check_gradle_report(path: Path, expected: int) -> None:
    validate_gradle_report(path.read_text(encoding="utf-8"), expected)


def validate_adb_trace(trace: str, expected: int) -> None:
    """Require unique test start/pass pairs, declared count, and normal runner end."""
    started: set[str] = set()
    finished: set[str] = set()
    current: str | None = None
    declared: set[int] = set()
    endings: list[int] = []
    for line in trace.splitlines():
        if line.startswith("INSTRUMENTATION_STATUS: test="):
            current = line.partition("=")[2].strip()
            if not current:
                raise ValueError("ADB runner emitted empty test name")
        elif line.startswith("INSTRUMENTATION_STATUS: numtests="):
            value = line.partition("=")[2].strip()
            if not value.isdecimal():
                raise ValueError(f"Invalid declared test count: {value}")
            declared.add(int(value))
        elif line.startswith("INSTRUMENTATION_STATUS_CODE:"):
            value = line.partition(":")[2].strip()
            if value not in ("0", "1"):
                raise ValueError(f"ADB runner reported test error/status {value}")
            if not current:
                raise ValueError("ADB runner reported test status without a test name")
            if value == "1":
                if current in started:
                    raise ValueError(f"ADB runner started test twice: {current}")
                started.add(current)
            else:
                if current not in started or current in finished:
                    raise ValueError(f"ADB runner completed unexpected/duplicate test: {current}")
                finished.add(current)
        elif line.startswith("INSTRUMENTATION_CODE:"):
            try:
                endings.append(int(line.partition(":")[2].strip()))
            except ValueError as exc:
                raise ValueError("ADB runner invalid terminal code") from exc
        elif line.startswith(("INSTRUMENTATION_FAILED:", "INSTRUMENTATION_ABORTED:")):
            raise ValueError(f"ADB runner failed: {line}")
    if declared != {expected}:
        raise ValueError(f"ADB runner declared totals {sorted(declared)}, expected {expected}")
    if len(started) != expected or finished != started:
        raise ValueError(f"ADB runner executed {len(finished)}/{expected} unique passing tests")
    if endings != [-1]:
        raise ValueError(f"ADB runner did not finish successfully: {endings}")
    if not re.search(r"(?m)^OK \(" + str(expected) + r" tests\)\s*$", trace):
        raise ValueError(f"ADB runner missing OK ({expected} tests) summary")
    print(f"PASS: ADB runner {expected} distinct tests, no failures, normal terminal code -1")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gradle-report", type=Path)
    parser.add_argument("--adb-log", type=Path)
    parser.add_argument("--expected-tests", type=int, default=8)
    args = parser.parse_args()
    if not args.gradle_report and not args.adb_log:
        parser.error("at least one of --gradle-report or --adb-log is required")
    if args.expected_tests < 1:
        parser.error("--expected-tests must be positive")
    try:
        if args.gradle_report:
            check_gradle_report(args.gradle_report, args.expected_tests)
        if args.adb_log:
            validate_adb_trace(args.adb_log.read_text(encoding="utf-8-sig"), args.expected_tests)
    except (ValueError, OSError) as exc:
        print(f"FAIL: Android instrumentation acceptance: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
