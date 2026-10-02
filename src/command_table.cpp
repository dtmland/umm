#include "command_table.hpp"

#include <sstream>

namespace umm_cli {

namespace {

const FlagSpec kBackend{"backend", 0, true, "BACKEND", "force a backend: exiv2 or exiftool"};
const FlagSpec kJson{"json", 0, false, "", "machine-readable JSON output"};
const FlagSpec kHelp{"help", 'h', false, "", "show help"};
const FlagSpec kRecursive{"recursive", 'r', false, "", "descend into directory operands"};
const FlagSpec kPolicy{"policy", 0, true, "POLICY", "embedded|sidecar|sidecar-required|preferred"};
const FlagSpec kDryRun{"dry-run", 0, false, "", "report what would change without writing"};

std::vector<CommandSpec> build() {
  return {
      {"read", "umm read [options] FILE...", "print all canonical metadata", Operands::files, 1,
       {{"sources", 0, false, "", "show provenance for each property"}}, true},
      {"get", "umm get [options] FILE PROPERTY...", "print named properties", Operands::file_then_args, 2,
       {}, true},
      {"set", "umm set [options] FILE ASSIGN...", "assign properties and persist",
       Operands::file_then_args, 2, {kPolicy, kDryRun}, true},
      {"rm", "umm rm [options] FILE PROPERTY...", "clear properties and persist",
       Operands::file_then_args, 2, {kPolicy, kDryRun}, true},
      {"unmapped", "umm unmapped [options] FILE...", "dump unmapped entries (read-only)",
       Operands::files, 1, {}, true},
      {"conflicts", "umm conflicts [options] FILE...", "list disagreeing properties", Operands::files, 1,
       {{"fail-on-conflict", 0, false, "", "exit non-zero when conflicts exist"}}, true},
      {"merge", "umm merge [options] FILE PROPERTY (--use RAWKEY | --value V)",
       "resolve a conflict and persist", Operands::file_then_args, 2,
       {{"use", 0, true, "RAWKEY", "choose a candidate source"},
        {"value", 0, true, "V", "supply an override value"},
        {"container", 0, true, "WHERE", "embedded|sidecar when a raw key is ambiguous"}, kPolicy,
        kDryRun},
       true},
      {"sync", "umm sync [options] FILE...", "make embedded and sidecar carriers agree",
       Operands::files, 1,
       {{"direction", 0, true, "DIR", "both|embedded-to-sidecar|sidecar-to-embedded"}, kDryRun},
       true},
      {"caps", "umm caps [options] FILE|TYPE...", "show per-backend capabilities", Operands::args, 1, {},
       false},
      {"geotag", "umm geotag --track TRACK [options] FILE...", "write GPS from a track log",
       Operands::files, 1,
       {{"track", 0, true, "TRACK", "GPX/NMEA/KML track file"},
        {"offset", 0, true, "MINUTES", "naive UTC offset in minutes (not clock skew)"}, kPolicy,
        kDryRun},
       true},
      {"doctor", "umm doctor [options]", "report backend availability and ExifTool discovery",
       Operands::none, 0, {}, false},
      {"setup", "umm setup exiftool", "install ExifTool for the current user", Operands::args, 1,
       {}, false},
      {"version", "umm version [options]", "tool, libumm, and standards versions", Operands::none, 0, {},
       false},
  };
}

}  // namespace

const std::vector<FlagSpec>& global_flags() {
  static const std::vector<FlagSpec> g{kBackend, kJson, kHelp};
  return g;
}

const FlagSpec& recursive_flag() { return kRecursive; }

const std::vector<CommandSpec>& commands() {
  static const std::vector<CommandSpec> table = build();
  return table;
}

const CommandSpec* find_command(std::string_view name) {
  for (const CommandSpec& c : commands())
    if (c.name == name) return &c;
  return nullptr;
}

const FlagSpec* find_flag(const CommandSpec& cmd, std::string_view name, bool long_form) {
  auto match = [&](const FlagSpec& f) {
    return long_form ? f.name == name : (f.short_name != 0 && name.size() == 1 && name[0] == f.short_name);
  };
  for (const FlagSpec& f : cmd.flags)
    if (match(f)) return &f;
  for (const FlagSpec& f : global_flags())
    if (match(f)) return &f;
  if (cmd.batch_files)
    if (match(kRecursive)) return &kRecursive;
  return nullptr;
}

namespace {
void print_flag(std::ostringstream& os, const FlagSpec& f) {
  std::string left = "  ";
  if (f.short_name) left += std::string("-") + f.short_name + ", ";
  left += "--" + std::string(f.name);
  if (f.takes_value) left += " " + std::string(f.value_name);
  os << left;
  for (std::size_t i = left.size(); i < 32; ++i) os << ' ';
  os << ' ' << f.summary << '\n';
}
}  // namespace

std::string top_level_help() {
  std::ostringstream os;
  os << "umm - media metadata tool built on libumm\n\n"
     << "Usage: umm [global options] COMMAND [options] [operands]\n\nCommands:\n";
  for (const CommandSpec& c : commands()) {
    std::string left = "  " + std::string(c.name);
    os << left;
    for (std::size_t i = left.size(); i < 14; ++i) os << ' ';
    os << c.summary << '\n';
  }
  os << "\nGlobal options:\n";
  for (const FlagSpec& f : global_flags()) print_flag(os, f);
  os << "\nRun 'umm COMMAND --help' for command usage.\n";
  return os.str();
}

std::string command_help(const CommandSpec& cmd) {
  std::ostringstream os;
  os << "Usage: " << cmd.synopsis << "\n\n" << cmd.summary << "\n\nOptions:\n";
  for (const FlagSpec& f : cmd.flags) print_flag(os, f);
  for (const FlagSpec& f : global_flags()) print_flag(os, f);
  if (cmd.batch_files) print_flag(os, kRecursive);
  if (cmd.name == "geotag") {
    os << "\n--track is required. Formats are those umm::importTrack accepts "
          "(GPX/NMEA/KML); the CLI does not parse tracks.\n"
          "--offset sets MatchOptions::naive_utc_offset_minutes when the capture "
          "time has no zone. Units are minutes (0 treats naive times as UTC).\n";
  }
  if (cmd.name == "setup") {
    os << "\nInstalls ExifTool for the current user and records the path in the "
          "umm config file (exiftool key). Does not modify PATH. umm never "
          "bundles ExifTool.\n\n"
          "Windows: winget install -e --id OliverBetz.ExifTool (standalone "
          "exiftool.exe, no Perl). Fallback: checksum-verified upstream .zip "
          "into a per-user prefix.\n"
          "Linux: apt (libimage-exiftool-perl), dnf (perl-Image-ExifTool), "
          "pacman (perl-image-exiftool); else checksum-verified tarball.\n"
          "macOS: brew install exiftool; else checksum-verified tarball.\n";
  }
  if (cmd.name == "doctor") {
    os << "\nReports which backends are usable, which ExifTool (and Perl, where "
          "relevant) was found and via which discovery step (config / "
          "UMM_EXIFTOOL / PATH), and umm setup exiftool remediation when "
          "ExifTool is missing.\n";
  }
  return os.str();
}

}  // namespace umm_cli
