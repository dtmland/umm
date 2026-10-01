#include <iostream>
#include <string>
#include <vector>

#include "cli.hpp"

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  return umm_cli::run(args, std::cout, std::cerr);
}
