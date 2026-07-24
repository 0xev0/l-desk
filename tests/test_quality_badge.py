#!/usr/bin/env python3

import subprocess
import tempfile
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET


GENERATOR = Path(__file__).resolve().parents[1] / "tools" / "render_test_badge.py"


class QualityBadgeTest(unittest.TestCase):
    def render(self, junit: str) -> str:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            junit_path = root / "results.xml"
            output_path = root / "badge.svg"
            junit_path.write_text(junit, encoding="utf-8")
            subprocess.run(
                [
                    "python3",
                    str(GENERATOR),
                    "--junit",
                    str(junit_path),
                    "--output",
                    str(output_path),
                ],
                check=True,
            )
            result = output_path.read_text(encoding="utf-8")
            ET.fromstring(result)
            return result

    def test_passing_and_failing_results_select_truthful_status(self):
        passing = self.render(
            '<testsuite tests="28" failures="0" errors="0" skipped="0"/>'
        )
        self.assertIn("100% passing", passing)
        self.assertIn("#18866f", passing)

        failing = self.render(
            '<testsuites><testsuite tests="4" failures="1" errors="0" skipped="1"/>'
            "</testsuites>"
        )
        self.assertIn("50% passing", failing)
        self.assertIn("#b83c32", failing)

    def test_empty_results_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            junit_path = root / "results.xml"
            junit_path.write_text('<testsuite tests="0"/>', encoding="utf-8")
            result = subprocess.run(
                [
                    "python3",
                    str(GENERATOR),
                    "--junit",
                    str(junit_path),
                    "--output",
                    str(root / "badge.svg"),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
