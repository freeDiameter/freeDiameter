#!/usr/bin/env python3
"""Focused tests for the documentation link checker."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

from check_doc_links import check_file


class LinkTests(unittest.TestCase):
    def test_existing_links_and_nonlocal_links(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "guide.md"
            (path.parent / "other.md").touch()
            path.write_text(
                "[local](other.md#section) [remote](https://example.org/a)\n"
                "[anchor](#section)\n```\n[example](missing.md)\n```\n",
                encoding="utf-8",
            )
            self.assertEqual(0, check_file(path))

    def test_missing_link_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "guide.md"
            path.write_text("[bad](missing.md)\n", encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(1, check_file(path))
            self.assertIn("guide.md:1: missing link missing.md", output.getvalue())


if __name__ == "__main__":
    unittest.main()
