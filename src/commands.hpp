#pragma once

#include <iosfwd>

#include "args.hpp"
#include "errors.hpp"

namespace umm_cli {

// Runs a parsed command. Commands through session 09 are implemented
// (`version`, `read`, `get`, `unmapped`, `conflicts`, `caps`, `set`, `rm`,
// `merge`, `sync`); remaining commands validate operands then report
// "not implemented".
ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
