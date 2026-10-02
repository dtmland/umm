#!/usr/bin/env python3
"""Generate draft GitHub release notes from in-repo pins (session 14)."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CMAKE = REPO_ROOT / "CMakeLists.txt"
LIBUMM_ENV = REPO_ROOT / "tools" / "build" / "libumm.env"
EXIFTOOL_ENV = REPO_ROOT / "tools" / "build" / "exiftool.env"
MANIFEST = REPO_ROOT / "tools" / "build" / "corresponding-source.json"
PROJECT_VERSION_RE = re.compile(
    r"project\(\s*umm\s+VERSION\s+(\d+\.\d+\.\d+)", re.IGNORECASE
)
KEY_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")


def die(message: str) -> None:
    raise SystemExit(f"generate_release_notes.py: {message}")


def cli_version(path: Path = CMAKE) -> str:
    text = path.read_text(encoding="utf-8")
    match = PROJECT_VERSION_RE.search(text)
    if not match:
        die(f"{path} has no project(umm VERSION x.y.z)")
    return match.group(1)


def parse_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().lstrip("\ufeff")
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            die(f"malformed line in {path}: {raw_line!r}")
        key, value = line.split("=", 1)
        if not KEY_RE.fullmatch(key):
            die(f"invalid key in {path}: {key!r}")
        values[key] = value
    return values


def load_manifest(path: Path = MANIFEST) -> dict[str, dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    components = data.get("components")
    if not isinstance(components, list) or not components:
        die(f"{path} has no components")
    return {item["id"]: item for item in components}


def render_notes() -> str:
    version = cli_version()
    libumm = parse_env(LIBUMM_ENV)
    exiftool = parse_env(EXIFTOOL_ENV)
    pins = load_manifest()
    return f"""# umm {version}

## Licensing

Binary artifacts that include the Exiv2 backend (compiled in via libumm) are
conveyed under **GPL-3.0**. umm's own source remains **Apache-2.0** (available
in the source archive and the git repository).

ExifTool is located at runtime and is **never bundled**. Use `umm setup
exiftool` (scripts in `install/` inside the binary archive) to acquire it.

## Pins

- libumm {libumm['UMM_LIBUMM_VERSION']} (statically linked)
- Exiv2 {pins['exiv2']['version']} (statically linked; corresponding source attached)
- Expat {pins['expat']['version']} (corresponding source attached)
- zlib {pins['zlib']['version']} (corresponding source attached)
- ExifTool {exiftool['UMM_EXIFTOOL_VERSION']} (not redistributed; doctor advisory)

Corresponding source for Exiv2, Expat, and zlib is attached as the pinned
upstream tarballs (same pins as the libumm release named above). `SHA256SUMS`
covers every release asset. There is no package-manager publication of umm
in v1.
"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--version-only",
        action="store_true",
        help="Print the CMake project version triple and exit",
    )
    args = parser.parse_args(argv)
    if args.version_only:
        print(cli_version())
        return 0
    text = render_notes()
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
