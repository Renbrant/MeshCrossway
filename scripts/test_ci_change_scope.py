#!/usr/bin/env python3
"""Offline tests for the no-firmware-build classifier."""
import unittest
from ci_change_scope import is_docs_only


class ScopeTests(unittest.TestCase):
    def test_docs_and_site_are_docs_only(self):
        self.assertTrue(is_docs_only([
            "README.md", "docs/contextual-ui-branding-v076.md",
            "docs/site/index.html", "docs/site/flasher/index.html",
            "docs/ui-prototype/index.html", "docs/images/logo.svg",
        ]))

    def test_changes_to_firmware_or_build_need_compile(self):
        for path in (
            "firmware/display-prototype/meshcrossway_ui.h",
            ".github/workflows/firmware-preflight.yml",
            "scripts/write_firmware_version.py",
            "scripts/ci_change_scope.py",
            "docs/site/flasher/firmware/versions/v0.7.6/manifest.json",
        ):
            self.assertFalse(is_docs_only(["README.md", path]), path)

    def test_empty_and_malformed_fail_closed(self):
        for files in ([], [""], ["docs/../../scripts/file.py"], ["firmware.txt"]):
            self.assertFalse(is_docs_only(files))


if __name__ == "__main__":
    unittest.main(verbosity=2)
