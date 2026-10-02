// Build-time generator for umm(1) and bash/zsh/fish completions.
// Source of truth: the CLI command table (and libumm accessor names).
#pragma once

#include <filesystem>
#include <string>

namespace umm_cli {

std::string generate_man_page();
std::string generate_bash_completion();
std::string generate_zsh_completion();
std::string generate_fish_completion();

// Writes umm.1 and completions/{umm.bash,_umm,umm.fish} under outdir.
void write_generated_docs(const std::filesystem::path& outdir);

}  // namespace umm_cli
