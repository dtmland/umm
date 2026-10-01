#pragma once

#include <iosfwd>

#include "args.hpp"
#include "errors.hpp"

namespace umm_cli {

// Runs a parsed command. `version`, `read`, and `get` are implemented
// (session 06); remaining commands validate operands then report
// "not implemented" until their session.
ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
