#include "args.hpp"

namespace umm_cli {

namespace {

void apply(ParsedArgs& out, const FlagSpec& f, const std::string& value) {
  if (f.name == "backend") {
    if (value != "exiv2" && value != "exiftool") {
      out.error = "--backend must be 'exiv2' or 'exiftool', got '" + value + "'";
      return;
    }
    out.backend = value;
  } else if (f.name == "json") {
    out.json = true;
  } else if (f.name == "help") {
    out.help = true;
  } else if (f.name == "recursive") {
    out.recursive = true;
  } else {
    out.options[std::string(f.name)] = value;
  }
}

}  // namespace

ParsedArgs parse_args(const std::vector<std::string>& argv) {
  ParsedArgs out;
  std::size_t i = 0;

  // Global flags may precede the command name.
  auto parse_flag = [&](const CommandSpec* cmd) -> bool {
    const std::string& a = argv[i];
    bool long_form = a.rfind("--", 0) == 0;
    std::string body = a.substr(long_form ? 2 : 1);
    std::string inline_value;
    bool has_inline = false;
    if (long_form) {
      auto eq = body.find('=');
      if (eq != std::string::npos) {
        inline_value = body.substr(eq + 1);
        body.resize(eq);
        has_inline = true;
      }
    }
    const FlagSpec* f = nullptr;
    if (cmd) {
      f = find_flag(*cmd, body, long_form);
    } else {
      for (const FlagSpec& g : global_flags())
        if (long_form ? g.name == body : (g.short_name && body.size() == 1 && body[0] == g.short_name)) f = &g;
    }
    if (!f) {
      out.error = "unknown option '" + a + "'";
      return false;
    }
    std::string value;
    if (f->takes_value) {
      if (has_inline) {
        value = inline_value;
      } else if (i + 1 < argv.size()) {
        value = argv[++i];
      } else {
        out.error = "option '--" + std::string(f->name) + "' requires a value";
        return false;
      }
    } else if (has_inline) {
      out.error = "option '--" + std::string(f->name) + "' does not take a value";
      return false;
    }
    apply(out, *f, value);
    return out.error.empty();
  };

  while (i < argv.size() && argv[i].size() > 1 && argv[i][0] == '-' && argv[i] != "--") {
    if (!parse_flag(nullptr)) return out;
    ++i;
  }
  if (i >= argv.size()) {
    if (!out.help) out.error = "no command given";
    return out;
  }
  if (out.help && argv[i] == "help") ++i;  // "umm --help help" is just help
  if (i >= argv.size()) return out;

  const CommandSpec* cmd = find_command(argv[i]);
  if (!cmd) {
    if (out.help) return out;
    out.error = "unknown command '" + argv[i] + "'";
    return out;
  }
  out.command = cmd;
  ++i;

  bool only_operands = false;
  for (; i < argv.size(); ++i) {
    const std::string& a = argv[i];
    if (!only_operands && a == "--") {
      only_operands = true;
    } else if (!only_operands && a.size() > 1 && a[0] == '-') {
      if (!parse_flag(cmd)) return out;
    } else {
      out.operands.push_back(a);
    }
  }
  if (out.help) return out;
  if (out.operands.size() < cmd->min_operands)
    out.error = "'" + std::string(cmd->name) + "' needs at least " + std::to_string(cmd->min_operands) +
                " operand(s)";
  return out;
}

}  // namespace umm_cli
