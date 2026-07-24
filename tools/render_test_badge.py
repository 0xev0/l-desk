#!/usr/bin/env python3

"""Render a deterministic local badge from CTest JUnit results."""

from __future__ import annotations

import argparse
from html import escape
from pathlib import Path
import xml.etree.ElementTree as ET


def integer_attribute(element: ET.Element, name: str) -> int:
    return int(element.attrib.get(name, "0"))


def result_counts(path: Path) -> tuple[int, int, int]:
    root = ET.parse(path).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.findall("testsuite"))
    total = sum(integer_attribute(suite, "tests") for suite in suites)
    failed = sum(
        integer_attribute(suite, "failures") + integer_attribute(suite, "errors")
        for suite in suites
    )
    skipped = sum(integer_attribute(suite, "skipped") for suite in suites)
    if total <= 0 or failed < 0 or skipped < 0 or failed + skipped > total:
        raise ValueError("invalid or empty JUnit result")
    return total, failed, skipped


def render_svg(total: int, failed: int, skipped: int) -> str:
    passed = total - failed - skipped
    percentage = 100.0 * passed / total
    message = f"{percentage:.0f}% passing"
    color = "#18866f" if failed == 0 else "#b83c32"
    label = "host tests"
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="190" height="20"
     role="img" aria-label="{escape(label)}: {escape(message)}">
  <title>{escape(label)}: {escape(message)}</title>
  <linearGradient id="s" x2="0" y2="100%">
    <stop offset="0" stop-color="#fff" stop-opacity=".18"/>
    <stop offset="1" stop-opacity=".08"/>
  </linearGradient>
  <clipPath id="r"><rect width="190" height="20" rx="3"/></clipPath>
  <g clip-path="url(#r)">
    <rect width="80" height="20" fill="#50575c"/>
    <rect x="80" width="110" height="20" fill="{color}"/>
    <rect width="190" height="20" fill="url(#s)"/>
  </g>
  <g fill="#fff" text-anchor="middle"
     font-family="Verdana,DejaVu Sans,sans-serif" font-size="11">
    <text x="40" y="15" fill="#010101" fill-opacity=".3">{label}</text>
    <text x="40" y="14">{label}</text>
    <text x="135" y="15" fill="#010101" fill-opacity=".3">{message}</text>
    <text x="135" y="14">{message}</text>
  </g>
</svg>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    arguments = parser.parse_args()

    total, failed, skipped = result_counts(arguments.junit)
    passed = total - failed - skipped
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(render_svg(total, failed, skipped), encoding="utf-8")
    if arguments.summary is not None:
        with arguments.summary.open("a", encoding="utf-8") as stream:
            stream.write(
                f"## Host tests\n\n"
                f"{passed}/{total} passed, {failed} failed, {skipped} skipped.\n"
            )


if __name__ == "__main__":
    main()
