"""Offline contract tests for completions/man generation and goldens."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CMAKE = (ROOT / "CMakeLists.txt").read_text()
GOLDENS = ROOT / "tests" / "goldens"


class DocsGenContract(unittest.TestCase):
    def test_generator_and_install_rules(self):
        self.assertIn("umm-gen-docs", CMAKE)
        self.assertIn("tools/gen/docs_gen.cpp", CMAKE)
        self.assertIn("GNUInstallDirs", CMAKE)
        self.assertIn("bash-completion/completions", CMAKE)
        self.assertIn("zsh/site-functions", CMAKE)
        self.assertIn("fish/vendor_completions.d", CMAKE)
        self.assertIn("${CMAKE_INSTALL_MANDIR}/man1", CMAKE)
        self.assertIn("umm-generated-docs", CMAKE)

    def test_goldens_are_lf(self):
        if not GOLDENS.is_dir():
            return
        for path in GOLDENS.rglob("*"):
            if not path.is_file() or path.name.startswith("."):
                continue
            data = path.read_bytes()
            self.assertNotIn(b"\r", data, msg=str(path))
