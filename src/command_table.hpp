// The command table is the single source of truth for the CLI surface:
// dispatch, --help, and (session 12) shell completions and umm(1).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace umm_cli {

struct FlagSpec {
  std::string_view name;      // long name without leading dashes
  char short_name;            // 0 when none
  bool takes_value;
  std::string_view value_name;  // for help, e.g. "BACKEND"
  std::string_view summary;
};

// How positional operands are interpreted.
enum class Operands {
  none,           // no operands
  files,          // FILE... (globs expanded by the shell; directories with -r)
  file_then_args, // FILE followed by command arguments (PROPERTY..., ASSIGN...)
  args            // plain arguments, no per-file batch (caps FILE|TYPE, setup NAME)
};

struct CommandSpec {
  std::string_view name;
  std::string_view synopsis;
  std::string_view summary;
  Operands operands;
  std::size_t min_operands;
  std::vector<FlagSpec> flags;  // command-specific flags (globals are separate)
  bool batch_files;             // per-file loop over FILE operands
};

// Global flags accepted by every command.
const std::vector<FlagSpec>& global_flags();
// --recursive/-r, accepted by commands whose operands are files.
const FlagSpec& recursive_flag();

const std::vector<CommandSpec>& commands();
const CommandSpec* find_command(std::string_view name);

std::string top_level_help();
std::string command_help(const CommandSpec& cmd);

// Resolves a flag for a command (command-specific, global, then recursive).
// `long_form` is true for --name, false for -n.
const FlagSpec* find_flag(const CommandSpec& cmd, std::string_view name, bool long_form);

}  // namespace umm_cli
