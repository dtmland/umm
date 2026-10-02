#include <iostream>
#include <stdexcept>
#include <string>

#include "docs_gen.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: umm-gen-docs OUTDIR\n";
    return 1;
  }
  try {
    umm_cli::write_generated_docs(argv[1]);
  } catch (const std::exception& e) {
    std::cerr << "umm-gen-docs: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
