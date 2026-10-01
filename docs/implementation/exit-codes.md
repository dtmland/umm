# Exit-code contract (stub; session 15 promotes to user docs)

Implemented in `src/errors.{hpp,cpp}`. The CLI maps `umm::ErrorCode` groups
(`include/umm/result.hpp` in libumm v0.1.0) at the libumm boundary.

| Exit | Group | `umm::ErrorCode` values |
|---|---|---|
| 0 | success | — |
| 1 | usage | unknown command, bad/missing arguments, bad config file |
| 2 | I/O | `io_not_found`, `io_read_failed`, `io_write_failed` |
| 3 | format | `format_unrecognized`, `format_corrupt` |
| 4 | backend | `backend_unavailable`, `backend_failed`, `backend_timeout` |
| 5 | capability | `unsupported_type`, `unsupported_capability` |
| 6 | semantics | `conflict_unresolved`, `invalid_value`, `unknown_property` |
| 7 | not found | reserved: `umm get` property absent (session 06) |
| 64 | mixed | batch with failures in more than one group |
| 69 | not implemented | temporary: stub command until sessions 06–11 land |
| 70 | internal | `internal` |

## Batch rule

Files are processed sequentially (no parallel reads or writes in v1). Every
per-file failure is recorded and reported, processing continues, and the exit
code is: the shared group code when all failures are in one group, otherwise
64. Exit 69 is not a libumm group and is removed once all commands are real.
