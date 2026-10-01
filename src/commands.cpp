#include "commands.hpp"

#include <filesystem>
#include <ostream>

#include "batch.hpp"
#include "output.hpp"

namespace umm_cli {

namespace fs = std::filesystem;

namespace {

umm::Result<void> check_file(const fs::path& p) {
  std::error_code ec;
  if (!fs::exists(p, ec))
    return umm::Error{umm::ErrorCode::io_not_found, "no such file: " + p.string(), "", ""};
  if (fs::is_directory(p, ec))
    return umm::Error{umm::ErrorCode::io_read_failed,
                      "is a directory (use --recursive): " + p.string(), "", ""};
  return {};
}

}  // namespace

ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  const CommandSpec& cmd = *args.command;

  if (cmd.batch_files) {
    std::vector<std::string> file_operands = args.operands;
    if (cmd.operands == Operands::file_then_args) file_operands.resize(1);
    BatchSummary summary = run_batch(expand_operands(file_operands, args.recursive), check_file);
    if (!summary.failures.empty()) {
      if (args.json) {
        std::vector<FileReport> reports;
        for (const FileFailure& f : summary.failures)
          reports.push_back({f.path.string(), false, f.message, {}});
        out << make_document(std::string(cmd.name), reports).dump() << "\n";
      } else {
        for (const FileFailure& f : summary.failures)
          err << "umm " << cmd.name << ": " << f.message << " [" << group_name(f.code) << "]\n";
        err << "umm " << cmd.name << ": " << summary.failures.size() << " of " << summary.total
            << " file(s) failed\n";
      }
      return summary.exit_code();
    }
  }

  err << "umm " << cmd.name << ": not implemented yet\n";
  return ExitCode::not_implemented;
}

}  // namespace umm_cli
