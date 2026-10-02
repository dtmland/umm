// User configuration (concept §7.1): TOML with one v1 key, `exiftool`.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/backend.hpp"

namespace umm_cli {

enum class Platform { linux_like, windows, macos };
Platform current_platform() noexcept;

struct Config {
  std::filesystem::path exiftool;  // empty = not configured
};

using GetEnv = std::function<std::optional<std::string>(std::string_view)>;
GetEnv process_env();

// Candidate config files in search order; the first that exists wins.
//   linux/other: $XDG_CONFIG_HOME/umm/config.toml, else ~/.config/umm/config.toml
//   windows:     %APPDATA%\umm\config.toml
//   macos:       $XDG_CONFIG_HOME/umm/config.toml (only if set),
//                ~/Library/Application Support/umm/config.toml
std::vector<std::filesystem::path> config_candidates(Platform platform, const GetEnv& env);

struct ConfigParse {
  Config config;
  std::string error;  // non-empty on a syntax error
};

// Minimal TOML subset: comments, [tables] (ignored), and `key = "string"` or
// `key = 'literal'`. Unknown keys are ignored for forward compatibility.
ConfigParse parse_config(std::string_view text);

// Loads the first existing candidate. Missing config is not an error.
ConfigParse load_config(Platform platform, const GetEnv& env);

// Applies the config as libumm's explicit ExifTool path (discovery step 1).
// Never mutates PATH or the environment.
umm::ExifToolConfig to_libumm(const Config& config);

// First candidate path (created by `umm setup exiftool` even if missing).
std::filesystem::path primary_config_path(Platform platform, const GetEnv& env);

// Writes a minimal TOML with the `exiftool` key. Creates parent directories.
bool write_config_file(const std::filesystem::path& path, const Config& config, std::string* error);

enum class DiscoveryStep { config, env, path, not_found };
const char* discovery_step_name(DiscoveryStep step) noexcept;

struct ExifToolDiscovery {
  DiscoveryStep step{DiscoveryStep::not_found};
  std::filesystem::path path;
  std::filesystem::path perl;  // empty when not relevant (Windows .exe)
};

// CLI-side discovery (config → UMM_EXIFTOOL → PATH), matching libumm.
ExifToolDiscovery discover_exiftool(const Config& config, const GetEnv& env);

}  // namespace umm_cli
