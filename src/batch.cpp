#include "batch.hpp"

#include <algorithm>

namespace umm_cli {

namespace fs = std::filesystem;

ExitCode BatchSummary::exit_code() const {
  std::vector<ExitCode> codes;
  for (const FileFailure& f : failures) codes.push_back(f.code);
  return summarize(codes);
}

std::vector<fs::path> expand_operands(const std::vector<std::string>& operands, bool recursive) {
  std::vector<fs::path> out;
  for (const std::string& op : operands) {
    std::error_code ec;
    if (recursive && fs::is_directory(op, ec)) {
      std::vector<fs::path> found;
      fs::recursive_directory_iterator it(op, fs::directory_options::skip_permission_denied, ec), end;
      for (; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) found.push_back(it->path());
      std::sort(found.begin(), found.end());
      out.insert(out.end(), found.begin(), found.end());
    } else {
      out.emplace_back(op);
    }
  }
  return out;
}

BatchSummary run_batch(const std::vector<fs::path>& files,
                       const std::function<umm::Result<void>(const fs::path&)>& fn) {
  BatchSummary summary;
  for (const fs::path& file : files) {
    ++summary.total;
    umm::Result<void> r = fn(file);
    if (!r.ok())
      summary.failures.push_back({file, exit_code_for(r.error().code), r.error().message});
  }
  return summary;
}

}  // namespace umm_cli
