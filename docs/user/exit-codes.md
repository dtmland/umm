# Exit codes

The CLI maps `umm::ErrorCode` groups from libumm (`include/umm/result.hpp`)
at the library boundary. Public libumm calls return `umm::Result`; they do
not throw.

| Exit | Group | Typical causes |
|---|---|---|
| 0 | success | — |
| 1 | usage | unknown command, bad or missing arguments, malformed config file |
| 2 | I/O | `io_not_found`, `io_read_failed`, `io_write_failed` |
| 3 | format | `format_unrecognized`, `format_corrupt` |
| 4 | backend | `backend_unavailable`, `backend_failed`, `backend_timeout` |
| 5 | capability | `unsupported_type`, `unsupported_capability` |
| 6 | semantics | `conflict_unresolved`, `invalid_value`, `unknown_property` |
| 7 | not found | `umm get`: a requested property is absent |
| 64 | mixed | batch with failures in more than one group |
| 70 | internal | `internal` |

Exit 69 (`not-implemented`) is reserved for a stub command and is not
returned by the shipped command set.

`--fail-on-conflict` on `umm conflicts` uses the semantics group (6) when
conflicts exist.

## Batch rule

Files are processed sequentially (no parallel reads or writes in v1). Every
per-file failure is recorded and reported; processing continues. The process
exit code is the shared group when all failures are in one group, otherwise
64.

Human stderr lists each failure. `--json` still prints one document covering
every operand (`ok` / `error` per file).
