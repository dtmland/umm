// Exit-code contract (docs/implementation/exit-codes.md). The CLI maps
// umm::ErrorCode *groups* to process exit codes at the libumm boundary.
#pragma once

#include <string>
#include <vector>

#include "umm/result.hpp"

namespace umm_cli {

enum class ExitCode : int {
  ok = 0,
  usage = 1,
  io = 2,
  format = 3,
  backend = 4,
  capability = 5,
  semantics = 6,
  not_found = 7,        // `umm get`: property absent (session 06)
  mixed_failures = 64,  // batch failures from more than one group
  not_implemented = 69, // stub command; disappears as sessions 06-11 land
  internal = 70,
};

constexpr int to_int(ExitCode c) noexcept { return static_cast<int>(c); }

ExitCode exit_code_for(umm::ErrorCode code) noexcept;
const char* group_name(ExitCode code) noexcept;

// One batch: no failures -> ok; all failures in one group -> that group's
// code; failures in several groups -> mixed_failures.
ExitCode summarize(const std::vector<ExitCode>& failures) noexcept;

}  // namespace umm_cli
