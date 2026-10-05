#include "docs_gen.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "command_table.hpp"
#include "property.hpp"

#ifndef UMM_CLI_VERSION
#error "UMM_CLI_VERSION is set by CMake from project(umm VERSION)"
#endif

namespace umm_cli {
namespace fs = std::filesystem;

namespace {

std::vector<FlagSpec> flags_for(const CommandSpec& cmd) {
  std::vector<FlagSpec> out = cmd.flags;
  for (const FlagSpec& f : global_flags()) out.push_back(f);
  if (cmd.batch_files) out.push_back(recursive_flag());
  return out;
}

std::string_view value_choices(std::string_view flag) {
  if (flag == "backend") return "exiv2 exiftool";
  if (flag == "policy") return "embedded sidecar sidecar-required preferred";
  if (flag == "direction") return "both embedded-to-sidecar sidecar-to-embedded";
  if (flag == "container") return "embedded sidecar";
  return "";
}

std::string join_names(const std::vector<CommandSpec>& cmds) {
  std::string out;
  for (const CommandSpec& c : cmds) {
    if (!out.empty()) out += ' ';
    out += c.name;
  }
  return out;
}

std::string accessors_joined() {
  std::string out;
  for (auto n : accessor_names()) {
    if (!out.empty()) out += ' ';
    out += n;
  }
  out += " iptc.photo. iptc.video. exif.";
  return out;
}

bool property_command(std::string_view name) {
  return name == "get" || name == "set" || name == "rm";
}

std::string groff_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '\\')
      out += "\\\\";
    else if (c == '-')
      out += "\\-";
    else
      out += c;
  }
  return out;
}

std::string zsh_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == ':' || c == '\\' || c == '[' || c == ']' || c == '\'' || c == '"') out += '\\';
    out += c;
  }
  return out;
}

std::string fish_quote(std::string_view s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += '\'';
  return out;
}

void write_file(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write " + path.string());
  out << text;
  if (!out) throw std::runtime_error("write failed: " + path.string());
}

}  // namespace

std::string generate_man_page() {
  std::ostringstream os;
  os << ".TH UMM 1 \"\" \"umm " << UMM_CLI_VERSION << "\" \"User Commands\"\n"
     << ".SH NAME\n"
     << "umm \\- media metadata tool built on libumm\n"
     << ".SH SYNOPSIS\n"
     << ".B umm\n"
     << "[\\fIglobal options\\fR] \\fICOMMAND\\fR [\\fIoptions\\fR] [\\fIoperands\\fR]\n";
  for (const CommandSpec& c : commands()) {
    os << ".br\n.B " << groff_escape(c.synopsis) << "\n";
  }
  os << ".SH DESCRIPTION\n"
     << "umm is a thin command-line interface on libumm. It adds argument parsing,\n"
     << "output formatting, batch orchestration, and environment setup only.\n"
     << "Metadata semantics, reconciliation, and backend behavior come from libumm.\n"
     << "The vocabulary is IPTC Photo, IPTC Video Metadata Hub, and EXIF property\n"
     << "ids, plus libumm convenience accessors (creator, locationCreated, and the rest).\n"
     << "There is no umm write command; use set, rm, merge, sync, and geotag.\n"
     << "Base tags are displayed with umm dumpall / umm dumpunmapped and cannot be written.\n"
     << ".PP\n"
     << "On get, set, and rm, a name is either a convenience accessor or a full\n"
     << "canonical id (prefixes iptc.photo., iptc.video., exif.).\n"
     << ".SH COMMANDS\n";
  for (const CommandSpec& c : commands()) {
    os << ".TP\n.B " << groff_escape(c.name) << "\n"
       << groff_escape(c.summary) << "\n"
       << groff_escape(c.synopsis) << "\n";
    for (const FlagSpec& f : c.flags) {
      os << ".br\n\\fB\\-\\-" << groff_escape(f.name) << "\\fR";
      if (f.takes_value) os << " \\fI" << groff_escape(f.value_name) << "\\fR";
      os << "  " << groff_escape(f.summary) << "\n";
    }
  }
  os << ".SH GLOBAL OPTIONS\n";
  for (const FlagSpec& f : global_flags()) {
    os << ".TP\n.B \\-\\-" << groff_escape(f.name);
    if (f.short_name) os << ", \\-" << f.short_name;
    os << "\n" << groff_escape(f.summary) << "\n";
  }
  os << ".TP\n.B \\-\\-recursive, \\-r\n"
     << groff_escape(recursive_flag().summary)
     << " (file-batch commands only).\n"
     << ".SH EXIT STATUS\n"
     << "0 on success. Non-zero codes follow libumm error groups: usage 1, I/O 2,\n"
     << "format 3, backend 4, capability 5, semantics 6, absent property 7,\n"
     << "mixed batch failures 64, internal 70. The full table lives in the project\n"
     << "exit-code documentation.\n"
     << ".SH SEE ALSO\n"
     << ".BR exiftool (1),\n"
     << ".BR exiv2 (1)\n"
     << ".PP\n"
     << "exiftool and exiv2 are external tools. umm locates ExifTool at runtime\n"
     << "and never bundles it. Exiv2 is compiled into libumm when that backend is\n"
     << "enabled; it is not a separate umm install.\n";
  return os.str();
}

std::string generate_bash_completion() {
  const std::string cmds = join_names(commands());
  const std::string accessors = accessors_joined();
  std::ostringstream os;
  os << "# bash completion for umm — generated from the command table. Do not edit.\n"
     << "_umm() {\n"
     << "  local cur prev cmd i\n"
     << "  COMPREPLY=()\n"
     << "  cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
     << "  prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
     << "  local commands='" << cmds << "'\n"
     << "  cmd=\n"
     << "  for ((i = 1; i < COMP_CWORD; i++)); do\n"
     << "    case \"${COMP_WORDS[i]}\" in\n"
     << "      -*) ;;\n"
     << "      *) cmd=\"${COMP_WORDS[i]}\"; break ;;\n"
     << "    esac\n"
     << "  done\n"
     << "  case \"$prev\" in\n"
     << "    --backend) COMPREPLY=($(compgen -W 'exiv2 exiftool' -- \"$cur\")); return ;;\n"
     << "    --policy) COMPREPLY=($(compgen -W 'embedded sidecar sidecar-required preferred' -- \"$cur\")); return ;;\n"
     << "    --direction) COMPREPLY=($(compgen -W 'both embedded-to-sidecar sidecar-to-embedded' -- \"$cur\")); return ;;\n"
     << "    --container) COMPREPLY=($(compgen -W 'embedded sidecar' -- \"$cur\")); return ;;\n"
     << "  esac\n"
     << "  if [[ -z $cmd ]]; then\n"
     << "    if [[ $cur == -* ]]; then\n"
     << "      COMPREPLY=($(compgen -W '--backend --json --help -h' -- \"$cur\"))\n"
     << "    else\n"
     << "      COMPREPLY=($(compgen -W \"$commands\" -- \"$cur\"))\n"
     << "    fi\n"
     << "    return\n"
     << "  fi\n"
     << "  case \"$cmd\" in\n";
  for (const CommandSpec& c : commands()) {
    os << "    " << c.name << ")\n";
    os << "      local opts='";
    for (const FlagSpec& f : flags_for(c)) {
      os << " --" << f.name;
      if (f.short_name) os << " -" << f.short_name;
    }
    os << "'\n";
    if (c.name == "setup") {
      os << "      if [[ $cur == -* ]]; then COMPREPLY=($(compgen -W \"$opts\" -- \"$cur\")); else "
            "COMPREPLY=($(compgen -W 'exiftool' -- \"$cur\")); fi\n";
    } else if (property_command(c.name)) {
      os << "      local props='" << accessors << "'\n"
         << "      if [[ $cur == -* ]]; then COMPREPLY=($(compgen -W \"$opts\" -- \"$cur\")); else "
            "COMPREPLY=($(compgen -W \"$props\" -- \"$cur\")); fi\n";
    } else {
      os << "      if [[ $cur == -* ]]; then COMPREPLY=($(compgen -W \"$opts\" -- \"$cur\")); fi\n";
    }
    os << "      ;;\n";
  }
  os << "  esac\n"
     << "}\n"
     << "complete -o default -F _umm umm\n";
  return os.str();
}

std::string generate_zsh_completion() {
  std::ostringstream os;
  os << "#compdef umm\n"
     << "# zsh completion for umm — generated from the command table. Do not edit.\n\n"
     << "_umm() {\n"
     << "  local -a commands\n"
     << "  commands=(\n";
  for (const CommandSpec& c : commands()) {
    os << "    '" << c.name << ":" << zsh_escape(c.summary) << "'\n";
  }
  os << "  )\n"
     << "  local -a globals\n"
     << "  globals=(\n";
  for (const FlagSpec& f : global_flags()) {
    os << "    '--" << f.name << "[" << zsh_escape(f.summary) << "]";
    auto ch = value_choices(f.name);
    if (f.takes_value) {
      os << ":" << f.value_name;
      if (!ch.empty()) os << ":(" << ch << ")";
    }
    os << "'\n";
    if (f.short_name) {
      os << "    '-" << f.short_name << "[" << zsh_escape(f.summary) << "]'\n";
    }
  }
  os << "  )\n"
     << "  _arguments -C \\\n"
     << "    $globals \\\n"
     << "    '1:command:->cmds' \\\n"
     << "    '*::arg:->args'\n"
     << "  case $state in\n"
     << "    cmds) _describe -t commands command commands ;;\n"
     << "    args)\n"
     << "      case $words[1] in\n";
  for (const CommandSpec& c : commands()) {
    os << "        " << c.name << ")\n"
       << "          local -a opts\n"
       << "          opts=($globals\n";
    for (const FlagSpec& f : c.flags) {
      os << "            '--" << f.name << "[" << zsh_escape(f.summary) << "]";
      auto ch = value_choices(f.name);
      if (f.takes_value) {
        os << ":" << f.value_name;
        if (!ch.empty()) os << ":(" << ch << ")";
      }
      os << "'\n";
      if (f.short_name)
        os << "            '-" << f.short_name << "[" << zsh_escape(f.summary) << "]'\n";
    }
    if (c.batch_files) {
      const FlagSpec& r = recursive_flag();
      os << "            '--" << r.name << "[" << zsh_escape(r.summary) << "]'\n"
         << "            '-r[" << zsh_escape(r.summary) << "]'\n";
    }
    os << "          )\n";
    if (c.name == "setup") {
      os << "          _arguments $opts '*:tool:(exiftool)'\n";
    } else if (property_command(c.name)) {
      os << "          local -a props\n"
         << "          props=(";
      for (auto n : accessor_names()) os << n << " ";
      os << "iptc.photo. iptc.video. exif.)\n"
         << "          _arguments $opts '*:property:($props)'\n";
    } else if (c.operands == Operands::none) {
      os << "          _arguments $opts\n";
    } else {
      os << "          _arguments $opts '*:file:_files'\n";
    }
    os << "          ;;\n";
  }
  os << "      esac\n"
     << "      ;;\n"
     << "  esac\n"
     << "}\n\n"
     << "_umm \"$@\"\n";
  return os.str();
}

std::string generate_fish_completion() {
  const std::string cmds = join_names(commands());
  std::ostringstream os;
  os << "# fish completion for umm — generated from the command table. Do not edit.\n";
  os << "complete -c umm -n 'not __fish_seen_subcommand_from " << cmds << "' -l json -d "
     << fish_quote("machine-readable JSON output") << "\n";
  os << "complete -c umm -n 'not __fish_seen_subcommand_from " << cmds
     << "' -l backend -x -a 'exiv2 exiftool' -d " << fish_quote("force a backend: exiv2 or exiftool")
     << "\n";
  os << "complete -c umm -n 'not __fish_seen_subcommand_from " << cmds
     << "' -s h -l help -d " << fish_quote("show help") << "\n";
  for (const CommandSpec& c : commands()) {
    os << "complete -c umm -n 'not __fish_seen_subcommand_from " << cmds << "' -a " << c.name
       << " -d " << fish_quote(c.summary) << "\n";
  }
  for (const CommandSpec& c : commands()) {
    std::string seen = "__fish_seen_subcommand_from " + std::string(c.name);
    for (const FlagSpec& f : flags_for(c)) {
      os << "complete -c umm -n " << fish_quote(seen);
      if (f.short_name) os << " -s " << f.short_name;
      os << " -l " << f.name;
      auto ch = value_choices(f.name);
      if (f.takes_value) {
        os << " -x";
        if (!ch.empty()) os << " -a " << fish_quote(ch);
      }
      os << " -d " << fish_quote(f.summary) << "\n";
    }
    if (c.name == "setup") {
      os << "complete -c umm -n " << fish_quote(seen) << " -a exiftool\n";
    } else if (property_command(c.name)) {
      os << "complete -c umm -n " << fish_quote(seen) << " -a " << fish_quote(accessors_joined())
         << "\n";
    }
  }
  return os.str();
}

void write_generated_docs(const fs::path& outdir) {
  fs::create_directories(outdir / "completions");
  write_file(outdir / "umm.1", generate_man_page());
  write_file(outdir / "completions" / "umm.bash", generate_bash_completion());
  write_file(outdir / "completions" / "_umm", generate_zsh_completion());
  write_file(outdir / "completions" / "umm.fish", generate_fish_completion());
}

}  // namespace umm_cli
