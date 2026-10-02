#!/usr/bin/env python3
"""Offline contract tests for the session 14 release pipeline."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
RELEASE_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "release.yml"
PACKAGE_RELEASE = REPO_ROOT / "tools" / "build" / "package_release.py"
FETCH_SOURCE = REPO_ROOT / "tools" / "build" / "fetch_corresponding_source.py"
GENERATE_NOTES = REPO_ROOT / "tools" / "build" / "generate_release_notes.py"
CHECKLIST = REPO_ROOT / "docs" / "release-checklist.md"
MANIFEST = REPO_ROOT / "tools" / "build" / "corresponding-source.json"
NOTICES = REPO_ROOT / "THIRD-PARTY-NOTICES.md"
NOTICE = REPO_ROOT / "NOTICE.md"
LICENSE = REPO_ROOT / "LICENSE"
CMAKE = REPO_ROOT / "CMakeLists.txt"
# Dummy archive version for packaging tests — not CMake project(umm VERSION).
PACKAGE_TEST_VERSION = "9.9.9"
PINNED_RUNNERS = ("ubuntu-24.04", "windows-2025", "macos-15")
LATEST_RUNNERS = ("ubuntu-latest", "windows-latest", "macos-latest")
LICENSE_FILES = (
    REPO_ROOT / "licenses" / "GPL-2.0.txt",
    REPO_ROOT / "licenses" / "GPL-3.0.txt",
    REPO_ROOT / "licenses" / "Expat.txt",
    REPO_ROOT / "licenses" / "Zlib.txt",
)


def release_text() -> str:
    return RELEASE_WORKFLOW.read_text(encoding="utf-8")


def run_tool(args: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", *args],
        cwd=cwd or REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


class TestCliVersionSource(unittest.TestCase):
    """A CMake project() bump must be enough for umm CLI version tests."""

    def test_cmake_defines_cli_version_from_project(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        match = re.search(r"project\(\s*umm\s+VERSION\s+(\d+\.\d+\.\d+)", cmake)
        self.assertIsNotNone(match, "CMakeLists.txt must have project(umm VERSION x.y.z)")
        self.assertIn("UMM_CLI_VERSION", cmake)
        self.assertIn("${PROJECT_VERSION}", cmake)

    def test_cli_tests_use_macro_not_hardcoded_project_version(self) -> None:
        cli_test = (REPO_ROOT / "tests" / "cli" / "test_cli.cpp").read_text(encoding="utf-8")
        self.assertIn("UMM_CLI_VERSION", cli_test)
        self.assertNotRegex(cli_test, r'out\.find\("umm \d+\.\d+\.\d+"\)')

    def test_sources_do_not_fallback_to_a_stale_version(self) -> None:
        for rel in ("src/commands.cpp", "tools/gen/docs_gen.cpp"):
            text = (REPO_ROOT / rel).read_text(encoding="utf-8")
            self.assertIn("#ifndef UMM_CLI_VERSION", text)
            self.assertIn("#error", text)
            self.assertNotRegex(text, r'#define\s+UMM_CLI_VERSION\s+"')

    def test_packaging_dummy_is_not_the_project_version(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        match = re.search(r"project\(\s*umm\s+VERSION\s+(\d+\.\d+\.\d+)", cmake)
        self.assertIsNotNone(match)
        self.assertNotEqual(PACKAGE_TEST_VERSION, match.group(1))


class TestReleaseWorkflow(unittest.TestCase):
    def test_workflow_file_exists_and_parses(self) -> None:
        self.assertTrue(RELEASE_WORKFLOW.is_file(), f"missing {RELEASE_WORKFLOW}")
        text = release_text()
        self.assertIn("\non:", text)
        self.assertIn("permissions:", text)
        self.assertIn("jobs:", text)

    def test_triggers_tags_and_dispatch(self) -> None:
        text = release_text()
        self.assertIn("workflow_dispatch", text)
        self.assertIn("v*.*.*", text)
        self.assertIn("tags:", text)

    def test_pinned_runners_and_presets(self) -> None:
        text = release_text()
        for label in PINNED_RUNNERS:
            self.assertIn(label, text)
        for label in LATEST_RUNNERS:
            self.assertNotIn(label, text)
        self.assertIn("sh tools/build/pins.sh", text)
        self.assertIn("cmake --preset default", text)
        self.assertRegex(text, r"fail-fast:\s*false")
        self.assertIn("cmake --build --preset default", text)
        self.assertIn("ctest --preset default", text)
        self.assertIn("-DUMM_REQUIRE_EXIV2=ON", text)
        self.assertIn("-DUMM_REQUIRE_EXIFTOOL=ON", text)
        self.assertIn("python3 -m unittest discover -s tests/build", text)

    def test_never_uploads_on_failure(self) -> None:
        text = release_text()
        self.assertNotIn("if: always()", text)
        self.assertIn("if-no-files-found: error", text)
        self.assertIn("needs:", text)
        self.assertIn("corresponding-source", text)
        self.assertIn("SHA256SUMS", text)
        self.assertIn("--draft", text)
        test_at = text.index("ctest --preset default")
        package_at = text.index("package_release.py package")
        self.assertLess(test_at, package_at, "tests must run before packaging")
        upload_at = text.index("Upload binary archive")
        self.assertLess(package_at, upload_at)

    def test_attaches_corresponding_source_not_exiftool(self) -> None:
        text = release_text()
        self.assertIn("fetch_corresponding_source.py", text)
        self.assertIn("corresponding-source.json", text)
        self.assertIn("package_release.py", text)
        self.assertIn("generate_release_notes.py", text)
        self.assertIn("umm-${VERSION}-src.tar.gz", text)
        self.assertIn("gh release create", text)
        self.assertIn(
            "github.event_name == 'push' && startsWith(github.ref, 'refs/tags/v')",
            text,
        )
        self.assertIn("! grep -Ei 'exiftool' dist/SHA256SUMS", text)
        self.assertNotIn("exiftool.org", text)
        self.assertNotIn("Image-ExifTool", text)

    def test_no_package_manager_publish(self) -> None:
        text = release_text().lower()
        for needle in (
            "brew publish",
            "homebrew",
            "winget",
            "vcpkg",
            "conan",
            "pypi",
            "crates.io",
            "apt-put",
        ):
            self.assertNotIn(needle, text)

    def test_download_artifact_is_patched(self) -> None:
        # GHSA-cxww-7g56-2vh6: Zip Slip in @actions/download-artifact
        # >=4.0.0,<4.1.3. The floating @v4 tag is treated as 4.0.0 by scanners.
        text = release_text()
        self.assertNotRegex(text, r"download-artifact@v4(?![\d.])")
        pins = re.findall(r"download-artifact@v(\d+)\.(\d+)\.(\d+)", text)
        self.assertTrue(pins, "download-artifact must be pinned to a patch version")
        for major, minor, patch in pins:
            self.assertGreaterEqual(
                (int(major), int(minor), int(patch)),
                (4, 1, 3),
                "download-artifact must be >= 4.1.3",
            )

    def test_helper_scripts_and_checklist_exist(self) -> None:
        for path in (
            PACKAGE_RELEASE,
            FETCH_SOURCE,
            GENERATE_NOTES,
            CHECKLIST,
            MANIFEST,
            NOTICES,
        ):
            self.assertTrue(path.is_file(), f"missing {path}")
        checklist = CHECKLIST.read_text(encoding="utf-8")
        self.assertIn("CMakeLists.txt", checklist)
        self.assertIn("vX.Y.Z", checklist)
        self.assertIn("SHA256SUMS", checklist)
        self.assertIn("workflow_dispatch", checklist)
        self.assertIn("umm-", checklist)
        self.assertIn("project(umm VERSION", checklist)
        self.assertIn("libumm.env", checklist)
        self.assertIn("UMM_CLI_VERSION", checklist)


class TestNoticesAndPins(unittest.TestCase):
    def test_source_license_remains_apache(self) -> None:
        text = LICENSE.read_text(encoding="utf-8")
        self.assertIn("Apache License", text)
        self.assertIn("Version 2.0", text)

    def test_binary_conveyance_is_gpl3(self) -> None:
        notice = NOTICE.read_text(encoding="utf-8")
        third = NOTICES.read_text(encoding="utf-8")
        self.assertIn("GPL-3.0", notice)
        self.assertIn("Apache", notice)
        self.assertIn("THIRD-PARTY-NOTICES.md", notice)
        self.assertIn("GPL-3.0", third)
        self.assertIn("Exiv2", third)
        self.assertIn("Expat", third)
        self.assertIn("zlib", third)
        self.assertIn("never bundled", third.lower())
        self.assertNotIn("ExifTool is bundled", third)

    def test_license_texts_are_present(self) -> None:
        for path in LICENSE_FILES:
            self.assertTrue(path.is_file(), f"missing {path}")
            self.assertGreater(path.stat().st_size, 200)

    def test_cmake_installs_notices(self) -> None:
        text = CMAKE.read_text(encoding="utf-8")
        self.assertIn("THIRD-PARTY-NOTICES.md", text)
        self.assertIn("CMAKE_INSTALL_DOCDIR", text)
        self.assertIn("licenses/", text)

    def test_manifest_matches_notices_and_libumm_pins(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(data["schema"], "umm.corresponding-source/v1")
        ids = [item["id"] for item in data["components"]]
        self.assertEqual(ids, ["exiv2", "expat", "zlib"])
        self.assertNotIn("exiftool", ids)
        third = NOTICES.read_text(encoding="utf-8")
        for item in data["components"]:
            self.assertIn(item["version"], third)
            self.assertRegex(item["sha256"], r"^[0-9a-f]{64}$")
            self.assertTrue(item["url"].startswith("https://"))
            for rel in item["license_files"]:
                self.assertTrue((REPO_ROOT / rel).is_file(), rel)
        libumm_env = (REPO_ROOT / "tools" / "build" / "libumm.env").read_text(
            encoding="utf-8"
        )
        self.assertIn(data["libumm_version"], libumm_env)


class TestReleaseTools(unittest.TestCase):
    def test_notes_include_version_pins_and_gpl(self) -> None:
        result = run_tool([str(GENERATE_NOTES)])
        self.assertEqual(result.returncode, 0, result.stderr)
        text = result.stdout
        version = run_tool([str(GENERATE_NOTES), "--version-only"])
        self.assertEqual(version.returncode, 0, version.stderr)
        self.assertRegex(version.stdout.strip(), r"^\d+\.\d+\.\d+$")
        self.assertIn(version.stdout.strip(), text)
        self.assertIn("GPL-3.0", text)
        self.assertIn("Apache-2.0", text)
        self.assertIn("never bundled", text)
        self.assertIn("no package-manager publication", text)
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        pins = {item["id"]: item for item in manifest["components"]}
        self.assertIn(pins["exiv2"]["version"], text)

    def test_package_archive_contains_required_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prefix = root / "prefix"
            (prefix / "bin").mkdir(parents=True)
            (prefix / "share" / "man" / "man1").mkdir(parents=True)
            (prefix / "share" / "bash-completion" / "completions").mkdir(parents=True)
            (prefix / "share" / "zsh" / "site-functions").mkdir(parents=True)
            (prefix / "share" / "fish" / "vendor_completions.d").mkdir(parents=True)
            (prefix / "share" / "doc" / "umm" / "licenses").mkdir(parents=True)
            (prefix / "bin" / "umm").write_bytes(b"bin")
            (prefix / "share" / "man" / "man1" / "umm.1").write_text(".TH\n", encoding="utf-8")
            (prefix / "share" / "bash-completion" / "completions" / "umm").write_text(
                "# bash\n", encoding="utf-8"
            )
            (prefix / "share" / "zsh" / "site-functions" / "_umm").write_text(
                "# zsh\n", encoding="utf-8"
            )
            (prefix / "share" / "fish" / "vendor_completions.d" / "umm.fish").write_text(
                "# fish\n", encoding="utf-8"
            )
            (prefix / "share" / "doc" / "umm" / "LICENSE").write_text("L\n", encoding="utf-8")
            (prefix / "share" / "doc" / "umm" / "NOTICE.md").write_text("N\n", encoding="utf-8")
            (prefix / "share" / "doc" / "umm" / "THIRD-PARTY-NOTICES.md").write_text(
                "T\n", encoding="utf-8"
            )
            (prefix / "share" / "doc" / "umm" / "licenses" / "GPL-3.0.txt").write_text(
                "GPL\n", encoding="utf-8"
            )
            out = root / "out"
            result = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "package",
                    "--install-prefix",
                    str(prefix),
                    "--source-root",
                    str(REPO_ROOT),
                    "--version",
                    PACKAGE_TEST_VERSION,
                    "--os",
                    "ubuntu-24.04",
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            archive = out / f"umm-{PACKAGE_TEST_VERSION}-ubuntu-24.04.tar.gz"
            self.assertTrue(archive.is_file())
            with tarfile.open(archive, "r:gz") as tar:
                names = tar.getnames()
            joined = "\n".join(names)
            prefix = f"umm-{PACKAGE_TEST_VERSION}"
            for needle in (
                f"{prefix}/bin/umm",
                f"{prefix}/share/man/man1/umm.1",
                f"{prefix}/share/bash-completion/completions/umm",
                f"{prefix}/share/zsh/site-functions/_umm",
                f"{prefix}/share/fish/vendor_completions.d/umm.fish",
                f"{prefix}/share/doc/umm/LICENSE",
                f"{prefix}/share/doc/umm/NOTICE.md",
                f"{prefix}/share/doc/umm/THIRD-PARTY-NOTICES.md",
                f"{prefix}/share/doc/umm/licenses/GPL-3.0.txt",
                f"{prefix}/README.md",
                f"{prefix}/install/exiftool.sh",
                f"{prefix}/install/exiftool.ps1",
                f"{prefix}/install/exiftool.bat",
                f"{prefix}/MANIFEST.txt",
            ):
                self.assertIn(needle, joined)
            self.assertNotIn("exiftool.exe", joined)
            self.assertNotRegex(joined, r"(?i)Image-ExifTool")
            missing = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "package",
                    "--install-prefix",
                    str(root / "empty"),
                    "--source-root",
                    str(REPO_ROOT),
                    "--version",
                    PACKAGE_TEST_VERSION,
                    "--os",
                    "ubuntu-24.04",
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertNotEqual(missing.returncode, 0)

    def test_sha256sums_covers_every_asset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "a.tar.gz").write_bytes(b"aaa")
            (directory / "b.tar.gz").write_bytes(b"bbb")
            output = directory / "SHA256SUMS"
            result = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "sha256sums",
                    "--dir",
                    str(directory),
                    "--output",
                    str(output),
                ]
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn("a.tar.gz", text)
            self.assertIn("b.tar.gz", text)
            self.assertNotRegex(text, r"  SHA256SUMS$")
            lines = [line for line in text.splitlines() if line]
            self.assertEqual(len(lines), 2)
            for line in lines:
                digest, name = line.split("  ", 1)
                actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
                self.assertEqual(digest, actual)

    def test_fetch_fail_closed_on_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = b"corresponding-source-fixture\n"
            archive = root / "good.tar.gz"
            archive.write_bytes(payload)
            digest = hashlib.sha256(payload).hexdigest()
            wrong = "0" * 64
            manifest = {
                "schema": "umm.corresponding-source/v1",
                "components": [
                    {
                        "id": "exiv2",
                        "name": "Exiv2",
                        "version": "0.0.0",
                        "url": archive.resolve().as_uri(),
                        "sha256": wrong,
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")
            out = root / "out"
            out.mkdir()
            dest = out / "exiv2-0.0.0.tar.gz"
            dest.write_bytes(b"stale")
            bad = run_tool(
                [
                    str(FETCH_SOURCE),
                    "--manifest",
                    str(manifest_path),
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertNotEqual(bad.returncode, 0, bad.stdout)
            self.assertIn("SHA-256 mismatch", bad.stderr)
            self.assertFalse(dest.exists())

            manifest["components"][0]["sha256"] = digest
            manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")
            good = run_tool(
                [
                    str(FETCH_SOURCE),
                    "--manifest",
                    str(manifest_path),
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertEqual(dest.read_bytes(), payload)


if __name__ == "__main__":
    unittest.main()
