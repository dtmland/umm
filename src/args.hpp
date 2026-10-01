#pragma once

#include <map>
#include <string>
#include <vector>

#include "command_table.hpp"

namespace umm_cli {

struct ParsedArgs {
  const CommandSpec* command{nullptr};  // null with help/no command
  std::string backend;                  // empty = libumm default
  bool json{false};
  bool recursive{false};
  bool help{false};
  std::vector<std::string> operands;
  // Command-specific flags: name -> value (empty for boolean flags).
  std::map<std::string, std::string> options;
  std::string error;  // non-empty => usage error
};

// Pure function over argv[1..]; never throws, never touches libumm.
ParsedArgs parse_args(const std::vector<std::string>& argv);

}  // namespace umm_cli
