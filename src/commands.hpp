#pragma once

#include <iosfwd>

#include "args.hpp"
#include "errors.hpp"

namespace umm_cli {

// Runs a parsed command. Commands through session 11 are implemented.
ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
