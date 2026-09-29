# Copilot instructions for umm

`umm` is the command-line media metadata tool built on **libumm**. It is to
libumm what `exiv2` is to Exiv2 or `exiftool` is to `Image::ExifTool`. The CLI
adds **no metadata logic of its own**: argument parsing, output formatting,
batch orchestration, and environment setup only. Everything semantic comes
from libumm.

## General

- Do not make claims without actually reading file contents — do not only look
  at file names and sizes, and do not speculate.
- Before deciding that a new dependency is needed that is not already in the
  project, perform due diligence to confirm that existing deps or tools cannot
  satisfy the need, and document the justification.
- Prefer the smallest, most surgical change that addresses the task. Do not
  refactor unrelated code or broaden scope without a clear reason.
- Follow the repo's explicit design records and implementation plan rather than
  inventing a new path.

## Repository conventions

- The founding design is [`docs/concept.md`](../docs/concept.md). It is
  authoritative for the feature set, libumm consumption model, backend rules,
  project skeleton, non-goals, and resolved planning questions.
- Session-sized work packages live under [`docs/implementation/`](../docs/implementation/).
  [`docs/implementation/00-overview.md`](../docs/implementation/00-overview.md)
  is the index, standing constraints, and concept-coverage map. When a session
  is assigned, implement **that session only**.
- After the initial plan is complete, new work follows `docs/concept.md` and
  any later decision records under `docs/analysis/` — not an informal path.
- The CLI vocabulary is libumm's standards-based canonical model (IPTC Photo,
  IPTC Video Metadata Hub, EXIF), not backend-specific tag names. Do not invent
  metadata properties or ad-hoc tag names on the command line. Unmapped
  *display* is `umm unmapped`; unmapped *write* is absent.
- Do not add metadata semantics, reconciliation, or backend behavior in this
  repo. Pass options through to libumm (`ReadOptions` / `WriteOptions` /
  `SyncOptions` / `MatchOptions`) and present libumm results.
- No exceptions across the libumm boundary. The CLI may use exceptions
  internally but must not rely on any thrown from libumm; public libumm calls
  return `umm::Result<T>`.
- Pins live in env files as the source of truth (`tools/build/libumm.env` for
  the libumm archive). Offline contract tests guard pins and workflows.
- Setup scripts are system-native (`.bat` / PowerShell and POSIX `sh`), never
  a Python helper. ExifTool is located at runtime and never bundled.
- Static linkage of libumm by default. Binary releases that contain the Exiv2
  backend are conveyed under GPL-3.0; CLI source in this repository remains
  Apache-2.0.
- The command table is the single source of truth for the CLI surface,
  generated completions, and `umm(1)`.
- Docs are organized by audience: `docs/user/`, `docs/sysadmin/`,
  `docs/developer/`; `docs/README.md` is the index.

## Implementation workflow

- Before changing behavior, read the exact files that define it; do not rely
  solely on names, grep hits, or assumptions from adjacent code.
- Before starting implementation work, read the assigned session document,
  `docs/implementation/00-overview.md`, and the concept sections that session
  cites. Check what earlier sessions already landed.
- Do not pull work from later sessions into the current one. If a prerequisite
  session is incomplete, stop and say so.
- When a change affects the CLI contract (commands, flags, exit codes, JSON
  schema, config keys), update the relevant session/design docs in the same
  change.
- Keep patches focused and testable. Prefer incremental, reviewable changes
  over broad rewrites.
- Validate the affected behavior using the smallest relevant existing tests or
  checks, and avoid adding new test tooling unless it is clearly necessary.
- Three-OS CI (Linux, Windows, macOS) with both libumm backends required is
  the project bar; do not weaken it.

## Crash-resistant progress

- For implementation tasks, push a checkpoint with `report_progress` after each
  coherent unit of work and whenever substantial changes have remained unpushed
  for 20 minutes.
- A local commit is not a durable checkpoint in the cloud-agent environment.
  Push the checkpoint branch even when the work is incomplete and identify
  unfinished or unvalidated work in the progress checklist.
- Run `git diff --check` before a checkpoint and exclude temporary or generated
  files from the patch.
- Do not create a pull request until implementation and validation are complete
  and the work is ready for final review. Until then, keep work on the feature
  branch as checkpoint commits only.

## Expected output quality

- Prefer clear, concrete changes with direct ties to the task and the repo's
  design docs.
- If a task is ambiguous, resolve the ambiguity by checking `docs/concept.md`
  and the relevant implementation session before proceeding.
- Preserve existing conventions and naming patterns in the codebase rather than
  introducing new ones unless the task explicitly requires it.
