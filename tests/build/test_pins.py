"""Offline contract tests for libumm pins (no network)."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENV = ROOT / "tools" / "build" / "libumm.env"
CMAKE = ROOT / "CMakeLists.txt"


def read_env():
    out = {}
    for line in ENV.read_text().splitlines():
        m = re.match(r"^([A-Z0-9_]+)=(.*)$", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


class PinContract(unittest.TestCase):
    def test_env_defines_pins(self):
        env = read_env()
        self.assertRegex(env.get("UMM_LIBUMM_VERSION", ""), r"^\d+\.\d+\.\d+")
        self.assertRegex(env.get("UMM_LIBUMM_SHA256", ""), r"^[0-9a-f]{64}$")
        self.assertTrue(env.get("UMM_LIBUMM_URL", "").startswith("https://"))

    def test_cmake_uses_env_pin_and_hash(self):
        text = CMAKE.read_text()
        self.assertIn("tools/build/libumm.env", text)
        self.assertIn("URL_HASH SHA256=", text)
        self.assertIn("${UMM_LIBUMM_URL}", text)
        self.assertNotRegex(text, r"URL\s+\"?https?://")
        self.assertNotIn("GIT_REPOSITORY", text)

    def test_system_switch_and_link(self):
        text = CMAKE.read_text()
        self.assertIn("UMM_CLI_USE_SYSTEM_LIBUMM", text)
        self.assertIn("find_package(umm CONFIG REQUIRED)", text)
        self.assertIn("umm::umm", text)

    def test_no_second_backend_acquisition(self):
        text = CMAKE.read_text().lower()
        for dep in ("exiv2", "expat", "zlib"):
            self.assertNotIn(dep, text)


if __name__ == "__main__":
    unittest.main()
