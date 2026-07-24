#!/usr/bin/env python3

"""Verify that repository-local Markdown links and images resolve."""

from pathlib import Path
import re
import unittest
from urllib.parse import unquote


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")


class DocumentationLinkTest(unittest.TestCase):
    def test_local_markdown_targets_exist(self):
        failures = []
        markdown_files = [
            path
            for path in REPOSITORY_ROOT.rglob("*.md")
            if ".git" not in path.parts
            and not any(part.startswith("build") for part in path.parts)
        ]
        for document in markdown_files:
            text = document.read_text(encoding="utf-8")
            for raw_target in MARKDOWN_LINK.findall(text):
                target = unquote(raw_target.strip("<>"))
                if (
                    "://" in target
                    or target.startswith(("mailto:", "#", "/", "../../"))
                ):
                    continue
                target_path = target.split("#", 1)[0]
                if not target_path:
                    continue
                resolved = (document.parent / target_path).resolve()
                if not resolved.exists():
                    failures.append(
                        f"{document.relative_to(REPOSITORY_ROOT)} -> {target_path}"
                    )
        self.assertEqual(failures, [], "\n".join(failures))


if __name__ == "__main__":
    unittest.main()
