"""Offline contract tests for install/ ExifTool setup scripts (session 11)."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INSTALL = ROOT / "install"


class SetupScripts(unittest.TestCase):
    def test_scripts_exist_and_are_not_python(self):
        bat = (INSTALL / "exiftool.bat").read_text(encoding="utf-8")
        ps1 = (INSTALL / "exiftool.ps1").read_text(encoding="utf-8")
        sh = (INSTALL / "exiftool.sh").read_text(encoding="utf-8")
        self.assertTrue((INSTALL / "exiftool.bat").is_file())
        self.assertTrue((INSTALL / "exiftool.ps1").is_file())
        self.assertTrue((INSTALL / "exiftool.sh").is_file())
        for text, name in ((bat, "bat"), (ps1, "ps1"), (sh, "sh")):
            self.assertNotIn("python", text.lower(), name)
            self.assertNotRegex(text, r"(?m)^#!.*python")

    def test_windows_uses_winget_oliver_betz(self):
        ps1 = (INSTALL / "exiftool.ps1").read_text(encoding="utf-8")
        bat = (INSTALL / "exiftool.bat").read_text(encoding="utf-8")
        self.assertIn("winget", ps1)
        self.assertIn("OliverBetz.ExifTool", ps1)
        self.assertIn("exiftool.exe", ps1)
        self.assertIn("Perl", ps1)
        self.assertIn("exiftool.ps1", bat)
        self.assertNotIn("python", bat.lower())

    def test_posix_mentions_package_managers(self):
        sh = (INSTALL / "exiftool.sh").read_text(encoding="utf-8")
        self.assertIn("apt", sh)
        self.assertIn("libimage-exiftool-perl", sh)
        self.assertIn("dnf", sh)
        self.assertIn("perl-Image-ExifTool", sh)
        self.assertIn("pacman", sh)
        self.assertIn("perl-image-exiftool", sh)
        self.assertIn("brew", sh)
        self.assertIn("exiftool", sh)

    def test_no_path_mutation_and_records_config(self):
        for name in ("exiftool.sh", "exiftool.ps1"):
            text = (INSTALL / name).read_text(encoding="utf-8")
            self.assertIn("exiftool =", text)
            self.assertIn("--record", text)
            self.assertNotIn("export PATH", text)
            self.assertNotIn("$env:PATH =", text)

    def test_no_exiftool_payload_in_tree(self):
        allowed = {
            INSTALL / "exiftool.bat",
            INSTALL / "exiftool.ps1",
            INSTALL / "exiftool.sh",
        }
        for folder in ("install", "src", "tests", "docs", "tools"):
            root = ROOT / folder
            if not root.is_dir():
                continue
            for p in root.rglob("*"):
                if not p.is_file() or p in allowed:
                    continue
                if p.suffix.lower() in {".exe", ".zip", ".pl"}:
                    self.fail(f"unexpected ExifTool payload: {p}")


if __name__ == "__main__":
    unittest.main()
