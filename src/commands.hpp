#pragma once

#include <iosfwd>

#include "args.hpp"
#include "errors.hpp"

namespace umm_cli {

// Runs a parsed command. Command bodies land in sessions 06-11; until then
// every command validates its operands through the batch driver and reports
// "not implemented" (ExitCode::not_implemented).
ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
