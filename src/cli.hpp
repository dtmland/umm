#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace umm_cli {

// Whole-program entry point over argv[1..]; returns the process exit code.
int run(const std::vector<std::string>& argv, std::ostream& out, std::ostream& err);

}  // namespace umm_cli
