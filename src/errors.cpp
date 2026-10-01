#include "errors.hpp"

namespace umm_cli {

ExitCode exit_code_for(umm::ErrorCode code) noexcept {
  using umm::ErrorCode;
  switch (code) {
    case ErrorCode::io_not_found:
    case ErrorCode::io_read_failed:
    case ErrorCode::io_write_failed:
      return ExitCode::io;
    case ErrorCode::format_unrecognized:
    case ErrorCode::format_corrupt:
      return ExitCode::format;
    case ErrorCode::backend_unavailable:
    case ErrorCode::backend_failed:
    case ErrorCode::backend_timeout:
      return ExitCode::backend;
    case ErrorCode::unsupported_type:
    case ErrorCode::unsupported_capability:
      return ExitCode::capability;
    case ErrorCode::conflict_unresolved:
    case ErrorCode::invalid_value:
    case ErrorCode::unknown_property:
      return ExitCode::semantics;
    case ErrorCode::internal:
      return ExitCode::internal;
  }
  return ExitCode::internal;
}

const char* group_name(ExitCode code) noexcept {
  switch (code) {
    case ExitCode::ok: return "success";
    case ExitCode::usage: return "usage";
    case ExitCode::io: return "io";
    case ExitCode::format: return "format";
    case ExitCode::backend: return "backend";
    case ExitCode::capability: return "capability";
    case ExitCode::semantics: return "semantics";
    case ExitCode::not_found: return "not-found";
    case ExitCode::mixed_failures: return "mixed";
    case ExitCode::not_implemented: return "not-implemented";
    case ExitCode::internal: return "internal";
  }
  return "internal";
}

ExitCode summarize(const std::vector<ExitCode>& failures) noexcept {
  if (failures.empty()) return ExitCode::ok;
  for (ExitCode c : failures)
    if (c != failures.front()) return ExitCode::mixed_failures;
  return failures.front();
}

}  // namespace umm_cli
