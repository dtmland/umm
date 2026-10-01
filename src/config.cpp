#include "config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace umm_cli {

namespace fs = std::filesystem;

Platform current_platform() noexcept {
#if defined(_WIN32)
  return Platform::windows;
#elif defined(__APPLE__)
  return Platform::macos;
#else
  return Platform::linux_like;
#endif
}

GetEnv process_env() {
  return [](std::string_view name) -> std::optional<std::string> {
    const char* v = std::getenv(std::string(name).c_str());
    if (!v || !*v) return std::nullopt;
    return std::string(v);
  };
}

std::vector<fs::path> config_candidates(Platform platform, const GetEnv& env) {
  std::vector<fs::path> out;
  auto home = env("HOME");
  auto add = [&](const fs::path& base) { out.push_back(base / "umm" / "config.toml"); };
  if (platform == Platform::windows) {
    if (auto appdata = env("APPDATA")) add(*appdata);
    return out;
  }
  if (auto xdg = env("XDG_CONFIG_HOME")) add(*xdg);
  if (home) {
    if (platform == Platform::macos)
      add(fs::path(*home) / "Library" / "Application Support");
    else if (!env("XDG_CONFIG_HOME"))
      add(fs::path(*home) / ".config");
  }
  return out;
}

namespace {

std::string trim(std::string_view s) {
  std::size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
  return std::string(s.substr(b, e - b));
}

// Parses a quoted value at the start of `v`; returns false on syntax error.
// Anything after the closing quote must be empty or a comment.
bool parse_string(const std::string& v, std::string& out) {
  if (v.empty()) return false;
  char q = v[0];
  if (q != '"' && q != '\'') return false;
  std::size_t i = 1;
  for (; i < v.size(); ++i) {
    char c = v[i];
    if (c == q) break;
    if (q == '"' && c == '\\') {
      if (++i >= v.size()) return false;
      switch (v[i]) {
        case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        default: return false;
      }
    } else {
      out += c;
    }
  }
  if (i >= v.size()) return false;
  std::string rest = trim(std::string_view(v).substr(i + 1));
  return rest.empty() || rest[0] == '#';
}

}  // namespace

ConfigParse parse_config(std::string_view text) {
  ConfigParse result;
  std::istringstream in{std::string(text)};
  std::string raw;
  int line_no = 0;
  while (std::getline(in, raw)) {
    ++line_no;
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#' || line[0] == '[') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      result.error = "line " + std::to_string(line_no) + ": expected key = \"value\"";
      return result;
    }
    std::string key = trim(std::string_view(line).substr(0, eq));
    std::string value = trim(std::string_view(line).substr(eq + 1));
    if (key == "exiftool") {
      std::string parsed;
      if (!parse_string(value, parsed)) {
        result.error = "line " + std::to_string(line_no) + ": 'exiftool' must be a quoted string";
        return result;
      }
      result.config.exiftool = parsed;
    }
  }
  return result;
}

ConfigParse load_config(Platform platform, const GetEnv& env) {
  for (const fs::path& p : config_candidates(platform, env)) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) continue;
    std::ifstream f(p, std::ios::binary);
    if (!f) {
      ConfigParse r;
      r.error = p.string() + ": cannot read config file";
      return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    ConfigParse r = parse_config(ss.str());
    if (!r.error.empty()) r.error = p.string() + ": " + r.error;
    return r;
  }
  return {};
}

umm::ExifToolConfig to_libumm(const Config& config) {
  umm::ExifToolConfig out;
  out.exiftool_script = config.exiftool;
  return out;
}

}  // namespace umm_cli
