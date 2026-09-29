# Session 14 — Release engineering

Status: **not started**

## Goal

Tag-triggered three-OS release pipeline copied from libumm's compliance
pattern: artifacts, notices, corresponding source. No package-manager
publication of umm in v1.

## Concept

- §4.1 GPL-3.0 conveyance for binaries that contain Exiv2; Apache-2.0 source;
  `THIRD-PARTY-NOTICES`, GPL-3.0 text, MIT/Zlib texts, pinned Exiv2/Expat/zlib
  source tarballs attached to each release. Exiv2-free Apache-only build not
  planned for v1.
- §4.2 umm distributes nothing of ExifTool
- §5 `.github/workflows/` release pipeline (artifacts + notices +
  corresponding source)
- §6 no package-manager publication of umm itself in v1
- §7.4 completions and `umm(1)` in release archives

## Prerequisites

Sessions 03 (CI) and 12 (completions/man). Notices can be drafted even if
12 is late, but archives should include man/completions when present.

## In scope

- `THIRD-PARTY-NOTICES.md` listing Exiv2 (GPL-3.0), Expat, zlib, and any
  other shipped third-party code. Follow libumm's current notice file;
  versions/pins must match what the pinned libumm actually vendors — read
  those files.
- Vendored license texts as libumm ships them (do not paraphrase licenses).
- Corresponding-source manifest: attach the **pinned** Exiv2/Expat/zlib
  source tarballs (the ones libumm's release uses) to each umm release.
  Prefer reusing libumm's pin URLs/hashes rather than inventing a second set.
- Tag-triggered **draft** release workflow:
  - Build umm on Linux, Windows, macOS
  - Static-linked CLI binary (+ completions + man page)
  - `SHA256SUMS`
  - Notices and license texts
  - Corresponding source attachments
  - Do **not** attach ExifTool
- Release archive names include OS and version.
- Offline contract tests: workflow is tag-triggered, three OSes, notices
  included, no ExifTool artifact, no package-manager publish step.
- `NOTICE.md` updated if session 01's stub is too thin.

## Out of scope

- Homebrew / winget / apt publication of **umm** (explicit v1 non-goal).
- Changing libumm's license decisions.
- User-facing install guide prose (session 15); this session can leave a
  bullet list of artifact names for that guide.

## Expected files

- `THIRD-PARTY-NOTICES.md`, license text copies as required
- `.github/workflows/release.yml` (name may vary)
- `tests/build/` release-workflow contract tests
- Optional `docs/release-checklist.md` stub for session 15

## Acceptance

- A test tag (or workflow_dispatch if that is how libumm debugs) produces
  draft assets matching the contract tests. If creating a real tag is
  undesirable in the session, the workflow file + contract tests + a dry
  listing of expected asset names is the bar; note what was not run.
- Binary conveyance story is explicit in notices: GPL-3.0 because of Exiv2.
- Source tree LICENSE remains Apache-2.0.

## Validation

- Offline unittest on workflow YAML and notice file presence.
- Read libumm's release workflow and notices and confirm umm copies the
  pattern, not a weaker one.
- `git diff --check`.
