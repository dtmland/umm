#pragma once

#include <iosfwd>

#include "args.hpp"
#include "errors.hpp"

namespace umm_cli {

// Runs a parsed command. Read-type commands through session 07 are
// implemented (`version`, `read`, `get`, `unmapped`, `conflicts`, `caps`);
// remaining commands validate operands then report "not implemented".
ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
