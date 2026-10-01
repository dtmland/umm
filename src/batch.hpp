// Sequential batch driver. No parallel reads or writes in v1.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "errors.hpp"
#include "umm/result.hpp"

namespace umm_cli {

struct FileFailure {
  std::filesystem::path path;
  ExitCode code;
  std::string message;
};

struct BatchSummary {
  std::size_t total{0};
  std::vector<FileFailure> failures;
  ExitCode exit_code() const;
};

// Expands directory operands when `recursive` (regular files, sorted for
// stable order). Non-existent operands are kept so the per-file step reports
// them. A directory without --recursive is kept as-is and fails per file.
std::vector<std::filesystem::path> expand_operands(const std::vector<std::string>& operands, bool recursive);

// Runs `fn` on each file in order; records failures and continues.
BatchSummary run_batch(const std::vector<std::filesystem::path>& files,
                       const std::function<umm::Result<void>(const std::filesystem::path&)>& fn);

}  // namespace umm_cli
