"""Source/receipt isolation checks; run directly without Blender or pytest."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/assets/prepare_smalltrotter_source.py"
SPEC = importlib.util.spec_from_file_location("smalltrotter_source_paths", SCRIPT)
PREP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREP)


class ReceiptPaths(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.source = self.folder / "Master.blend"
        self.source.write_bytes(b"harmless source fixture")

    def refused(self, receipt, mode="audit", output=None, watched=()):
        watched = (self.source, *watched)
        hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in watched}
        argv = [str(SCRIPT), "--input", str(self.source), "--mode", mode,
                "--receipt", str(receipt)]
        if output is not None:
            argv += ["--output", str(output)]
        stdout = io.StringIO()
        # No Blender dependency: refusal must happen before its availability
        # check, and before any success/failure receipt writer is reachable.
        with patch.object(sys, "argv", argv), patch.object(PREP, "bpy", None), contextlib.redirect_stdout(stdout):
            code = PREP.main()
        status = json.loads(stdout.getvalue())
        self.assertEqual(code, 2)
        self.assertFalse(status["success"])
        self.assertEqual(status["status"], "receipt_path_refused")
        for p, expected in hashes.items():
            self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(), expected)

    def test_receipt_source_refused_in_both_modes(self):
        for mode in ("audit", "seam-preparation"):
            with self.subTest(mode=mode):
                self.refused(self.source, mode, self.folder / "derived.blend")

    def test_receipt_source_relative_alias(self):
        self.refused(self.source.parent / "." / self.source.name)

    def test_receipt_output_refused_in_both_modes_before_creation(self):
        output = self.folder / "derived.blend"
        for mode in ("audit", "seam-preparation"):
            with self.subTest(mode=mode):
                self.refused(output, mode, output)
                self.assertFalse(output.exists())

    def test_case_alias_where_filesystem_supports_it(self):
        alias = self.source.with_name(self.source.name.swapcase())
        if not alias.exists() or not os.path.samefile(alias, self.source):
            self.skipTest("case-sensitive filesystem")
        self.refused(alias)

    def test_hardlink_source_alias_in_both_modes(self):
        alias = self.folder / "hardlink-receipt.json"
        try:
            os.link(self.source, alias)
        except OSError as exc:
            self.skipTest("hardlinks unavailable: " + str(exc))
        for mode in ("audit", "seam-preparation"):
            with self.subTest(mode=mode):
                self.refused(alias, mode, self.folder / "derived.blend", (alias,))

    def test_symlink_source_alias_in_both_modes(self):
        alias = self.folder / "symlink-receipt.json"
        try:
            alias.symlink_to(self.source)
        except OSError as exc:
            self.skipTest("symlinks unavailable: " + str(exc))
        for mode in ("audit", "seam-preparation"):
            with self.subTest(mode=mode):
                self.refused(alias, mode, self.folder / "derived.blend", (alias,))

    def test_hardlink_derivative_alias(self):
        output = self.folder / "existing-derived.blend"
        output.write_bytes(b"harmless derivative fixture")
        alias = self.folder / "derivative-receipt.json"
        try:
            os.link(output, alias)
        except OSError as exc:
            self.skipTest("hardlinks unavailable: " + str(exc))
        self.refused(alias, "seam-preparation", output, (output, alias))

    def test_distinct_receipt_path_is_allowed(self):
        output = self.folder / "derived.blend"
        receipt = self.folder / "receipt.json"
        self.assertIsNone(PREP.receipt_path_error(self.source, output, receipt))
        self.assertIsNone(PREP.receipt_path_error(self.source, None, receipt))
        self.assertIsNone(PREP.receipt_path_error(self.source, output, None))


if __name__ == "__main__":
    unittest.main()
