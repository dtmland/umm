#include "cli.hpp"

#include <ostream>

#include "args.hpp"
#include "command_table.hpp"
#include "commands.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "umm/backend.hpp"

namespace umm_cli {

int run(const std::vector<std::string>& argv, std::ostream& out, std::ostream& err) {
  ParsedArgs args = parse_args(argv);
  if (!args.error.empty()) {
    err << "umm: " << args.error << "\nTry 'umm --help'.\n";
    return to_int(ExitCode::usage);
  }
  if (args.help) {
    out << (args.command ? command_help(*args.command) : top_level_help());
    return to_int(ExitCode::ok);
  }

  ConfigParse cfg = load_config(current_platform(), process_env());
  if (!cfg.error.empty()) {
    err << "umm: config error: " << cfg.error << "\n";
    return to_int(ExitCode::usage);
  }
  if (!cfg.config.exiftool.empty())
    umm::BackendManager::instance().configureExifTool(to_libumm(cfg.config));

  return to_int(run_command(args, out, err));
}

}  // namespace umm_cli
