#!/usr/bin/env python3

"""Validate LLVM coverage JSON and publish a concise CI summary."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def metric(totals: dict, name: str) -> tuple[int, int, float]:
    value = totals.get(name)
    if not isinstance(value, dict):
        raise ValueError(f"missing coverage metric: {name}")
    count = int(value["count"])
    covered = int(value["covered"])
    percent = 100.0 if count == 0 else 100.0 * covered / count
    return count, covered, percent


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--minimum-lines", type=float, default=95.0)
    parser.add_argument("--summary", type=Path)
    arguments = parser.parse_args()

    payload = json.loads(arguments.input.read_text(encoding="utf-8"))
    totals = payload["data"][0]["totals"]
    line_count, line_covered, line_percent = metric(totals, "lines")
    function_count, function_covered, function_percent = metric(totals, "functions")
    branch_count, branch_covered, branch_percent = metric(totals, "branches")

    summary = (
        "## Host coverage\n\n"
        "| Metric | Covered | Total | Percentage |\n"
        "| --- | ---: | ---: | ---: |\n"
        f"| Lines | {line_covered} | {line_count} | {line_percent:.2f}% |\n"
        f"| Functions | {function_covered} | {function_count} | {function_percent:.2f}% |\n"
        f"| Branches | {branch_covered} | {branch_count} | {branch_percent:.2f}% |\n"
    )
    print(summary, end="")
    if arguments.summary is not None:
        with arguments.summary.open("a", encoding="utf-8") as stream:
            stream.write(summary)

    if line_percent < arguments.minimum_lines:
        raise SystemExit(
            f"line coverage {line_percent:.2f}% is below "
            f"{arguments.minimum_lines:.2f}%"
        )


if __name__ == "__main__":
    main()
