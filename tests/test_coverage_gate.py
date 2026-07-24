#!/usr/bin/env python3

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


CHECKER = Path(__file__).resolve().parents[1] / "tools" / "check_coverage.py"


def coverage_payload(lines_covered: int) -> dict:
    return {
        "data": [
            {
                "totals": {
                    "lines": {"count": 100, "covered": lines_covered},
                    "functions": {"count": 10, "covered": 10},
                    "branches": {"count": 20, "covered": 18},
                }
            }
        ]
    }


class CoverageGateTest(unittest.TestCase):
    def run_checker(self, covered: int) -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "coverage.json"
            path.write_text(json.dumps(coverage_payload(covered)), encoding="utf-8")
            return subprocess.run(
                [
                    "python3",
                    str(CHECKER),
                    "--input",
                    str(path),
                    "--minimum-lines",
                    "95",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_accepts_threshold_and_rejects_regression(self):
        accepted = self.run_checker(95)
        self.assertEqual(accepted.returncode, 0)
        self.assertIn("95.00%", accepted.stdout)

        rejected = self.run_checker(94)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("below 95.00%", rejected.stderr)


if __name__ == "__main__":
    unittest.main()
