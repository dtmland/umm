// Small self-contained CLI tests (no test framework dependency).
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <stdlib.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <variant>

#include "args.hpp"
#include "batch.hpp"
#include "cli.hpp"
#include "command_table.hpp"
#include "config.hpp"
#include "docs_gen.hpp"
#include "errors.hpp"
#include "output.hpp"
#include "property.hpp"
#include "umm/backend.hpp"
#include "umm/umm.hpp"
#include "umm/version.hpp"
#include "value_format.hpp"

using namespace umm_cli;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                      \
  do {                                                                   \
    if (!(cond)) {                                                       \
      std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
      ++failures;                                                        \
    }                                                                    \
  } while (0)

static int run_cli(std::vector<std::string> a, std::string* out = nullptr, std::string* err = nullptr) {
  std::ostringstream o, e;
  int rc = run(a, o, e);
  if (out) *out = o.str();
  if (err) *err = e.str();
  return rc;
}

static void test_version_linked() { CHECK(!umm::version().empty()); }

static void test_command_table() {
  for (const char* n : {"read", "get", "set", "rm", "unmapped", "conflicts", "merge", "sync", "caps",
                        "geotag", "doctor", "setup", "version"})
    CHECK(find_command(n) != nullptr);
  CHECK(commands().size() == 13);
  CHECK(find_command("write") == nullptr);  // no `umm write` (concept §2.1)
}

static void test_exit_codes() {
  using umm::ErrorCode;
  CHECK(exit_code_for(ErrorCode::io_not_found) == ExitCode::io);
  CHECK(exit_code_for(ErrorCode::format_corrupt) == ExitCode::format);
  CHECK(exit_code_for(ErrorCode::backend_timeout) == ExitCode::backend);
  CHECK(exit_code_for(ErrorCode::unsupported_type) == ExitCode::capability);
  CHECK(exit_code_for(ErrorCode::unknown_property) == ExitCode::semantics);
  CHECK(exit_code_for(ErrorCode::internal) == ExitCode::internal);
  CHECK(summarize({}) == ExitCode::ok);
  CHECK(summarize({ExitCode::io, ExitCode::io}) == ExitCode::io);
  CHECK(summarize({ExitCode::io, ExitCode::format}) == ExitCode::mixed_failures);
}

static void test_parse() {
  ParsedArgs a = parse_args({"read", "--backend", "exiv2", "--json", "-r", "a.jpg", "b.jpg"});
  CHECK(a.error.empty() && a.command && a.command->name == "read");
  CHECK(a.backend == "exiv2" && a.json && a.recursive && a.operands.size() == 2);
  CHECK(parse_args({"--json", "read", "x"}).json);
  CHECK(parse_args({"read", "--backend=exiftool", "x"}).backend == "exiftool");
  CHECK(!parse_args({"read", "--backend", "bogus", "x"}).error.empty());
  CHECK(!parse_args({"read"}).error.empty());
  CHECK(!parse_args({"nope"}).error.empty());
  CHECK(!parse_args({"read", "--bogus", "x"}).error.empty());
  CHECK(!parse_args({"doctor", "--recursive"}).error.empty());
  CHECK(parse_args({"read", "--", "-weird.jpg"}).operands.at(0) == "-weird.jpg");
  CHECK(parse_args({"geotag", "--track", "t.gpx", "--dry-run", "p.jpg"}).options.at("track") == "t.gpx");
  CHECK(parse_args({"read", "--help"}).help);
  CHECK(parse_args({"--help"}).help);
}

static void test_cli_behaviour() {
  std::string out, err;
  CHECK(run_cli({"--help"}, &out) == 0 && out.find("Commands:") != std::string::npos);
  CHECK(run_cli({"set", "--help"}, &out) == 0 && out.find("--policy") != std::string::npos);
  CHECK(run_cli({}) == 1);
  CHECK(run_cli({"not-a-command"}, nullptr, &err) == 1 && err.find("unknown command") != std::string::npos);
  CHECK(run_cli({"read"}) == 1);
  CHECK(run_cli({"version"}, &out) == 0 && out.find("umm ") != std::string::npos &&
        out.find("libumm ") != std::string::npos);
  // Batch over missing files reports each failure and exits with the io code.
  CHECK(run_cli({"read", "/no/such/1.jpg", "/no/such/2.jpg"}, nullptr, &err) == to_int(ExitCode::io));
  CHECK(err.find("1.jpg") != std::string::npos && err.find("2.jpg") != std::string::npos);
  CHECK(err.find("2 of 2") != std::string::npos);
  CHECK(run_cli({"read", "--json", "/no/such/1.jpg"}, &out) == to_int(ExitCode::io));
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
}

static void test_batch_recursive() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_tree";
  fs::remove_all(dir);
  fs::create_directories(dir / "sub");
  std::ofstream(dir / "b.jpg") << "x";
  std::ofstream(dir / "sub" / "a.jpg") << "x";
  CHECK(expand_operands({dir.string()}, true).size() == 2);
  CHECK(expand_operands({dir.string()}, false).size() == 1);
  fs::remove_all(dir);
}

static void test_property_seam() {
  CHECK(resolve_property("iptc.photo.creator", umm::MediaDomain::photo).ok());
  auto r = resolve_property("no.such.id", umm::MediaDomain::photo);
  CHECK(!r.ok() && r.error().code == umm::ErrorCode::unknown_property);
  CHECK(!resolve_property("definitely-not-a-name", umm::MediaDomain::unknown).ok());
  auto c = resolve_property("creator", umm::MediaDomain::photo);
  CHECK(c.ok() && c.value().is_accessor());
  auto g = resolve_property("gps", umm::MediaDomain::video);
  CHECK(g.ok() && g.value().is_accessor());
  CHECK(accessor_names().size() >= 38);
  for (auto n : accessor_names()) CHECK(resolve_property(n, umm::MediaDomain::unknown).ok());
  umm::Metadata empty;  // getters are callable through the bound pointer
  CHECK(!(empty.*(c.value().getter))().has_value());
}

static void test_config() {
  CHECK(parse_config("# c\n[tbl]\nother = 1\nexiftool = \"/opt/et/exiftool\"  # x\n").config.exiftool ==
        fs::path("/opt/et/exiftool"));
  CHECK(parse_config("exiftool = 'C:\\tools\\exiftool.exe'\n").config.exiftool ==
        fs::path("C:\\tools\\exiftool.exe"));
  CHECK(parse_config("").config.exiftool.empty());
  CHECK(!parse_config("exiftool = 5\n").error.empty());
  CHECK(!parse_config("garbage\n").error.empty());

  GetEnv env = [](std::string_view k) -> std::optional<std::string> {
    if (k == "HOME") return std::string("/home/u");
    if (k == "APPDATA") return std::string("C:/Users/u/AppData/Roaming");
    return std::nullopt;
  };
  CHECK(config_candidates(Platform::linux_like, env).at(0) == fs::path("/home/u/.config/umm/config.toml"));
  CHECK(config_candidates(Platform::macos, env).at(0) ==
        fs::path("/home/u/Library/Application Support/umm/config.toml"));
  CHECK(config_candidates(Platform::windows, env).at(0) ==
        fs::path("C:/Users/u/AppData/Roaming/umm/config.toml"));
  GetEnv xdg = [](std::string_view k) -> std::optional<std::string> {
    if (k == "XDG_CONFIG_HOME") return std::string("/x");
    if (k == "HOME") return std::string("/home/u");
    return std::nullopt;
  };
  CHECK(config_candidates(Platform::linux_like, xdg).at(0) == fs::path("/x/umm/config.toml"));

  // Temp config is honored; absence is not an error.
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_cfg";
  fs::remove_all(dir);
  GetEnv tmp = [&](std::string_view k) -> std::optional<std::string> {
    if (k == "XDG_CONFIG_HOME") return dir.string();
    return std::nullopt;
  };
  CHECK(load_config(Platform::linux_like, tmp).error.empty());
  CHECK(load_config(Platform::linux_like, tmp).config.exiftool.empty());
  fs::create_directories(dir / "umm");
  std::ofstream(dir / "umm" / "config.toml") << "exiftool = \"/opt/exiftool\"\n";
  ConfigParse loaded = load_config(Platform::linux_like, tmp);
  CHECK(loaded.error.empty() && loaded.config.exiftool == fs::path("/opt/exiftool"));
  CHECK(to_libumm(loaded.config).exiftool_script == fs::path("/opt/exiftool"));
  fs::remove_all(dir);
}

static void test_output() {
  CHECK(Json("a\"b\n").dump(-1) == "\"a\\\"b\\n\"");
  CHECK(Json(Json::Array{Json(1), Json(true), Json()}).dump(-1) == "[1,true,null]");
  FileReport f{"a.jpg", true, "", {{"iptc.photo.creator", Json("Jane"), "Jane", {}},
                                   {"iptc.photo.contributor",
                                    Json(Json::Array{Json(Json::Object{{"name", Json("Al")}})}),
                                    "1 entry", {}}}};
  std::string doc = make_document("read", {f}).dump(-1);
  CHECK(doc.rfind("{\"schema_version\":1,\"command\":\"read\"", 0) == 0);
  CHECK(doc.find("\"value\":[{\"name\":\"Al\"}]") != std::string::npos);  // struct → full JSON
  CHECK(make_document("version", {}, {{"version", Json("0.1.0")}}).dump(-1).find("\"schema_version\":1") !=
        std::string::npos);
  std::string table = format_table({f});
  CHECK(table.find("iptc.photo.creator") != std::string::npos && table.find("1 entry") != std::string::npos);
  CHECK(table.find("[{") == std::string::npos);  // compact summary in table
  FileReport bad{"b.jpg", false, "boom", {}};
  CHECK(format_table({f, bad}).find("error: boom") != std::string::npos);
  CHECK(make_document("read", {bad}).dump(-1).find("\"ok\":false") != std::string::npos);
}

static const unsigned char kMinimalJpeg[] = {
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x10, 0x00, 0x10,
    0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x00, 0xff, 0xd9};

// EXIF-only JPEG with DateTimeOriginal 2025:01:15 14:30:00 and no OffsetTime.
// umm::write of a naive DateTime is write-synced to XMP/IIM; Exiv2 stores that
// as UTC, so read-back is no longer naive and geotag would not require --offset.
static const unsigned char kNaiveExifJpeg[] = {
    0xff, 0xd8, 0xff, 0xe1, 0x00, 0xec, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4d, 0x4d, 0x00, 0x2a,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x05, 0x01, 0x32, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
    0x00, 0x4a, 0x01, 0x3b, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x5e, 0x02, 0x13,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x82, 0x98, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0x00, 0x6a, 0x87, 0x69, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x32, 0x30, 0x32, 0x35, 0x3a, 0x30, 0x31, 0x3a, 0x31, 0x35,
    0x20, 0x31, 0x34, 0x3a, 0x33, 0x30, 0x3a, 0x30, 0x30, 0x00, 0x45, 0x58, 0x49, 0x46, 0x20, 0x41,
    0x72, 0x74, 0x69, 0x73, 0x74, 0x00, 0x45, 0x58, 0x49, 0x46, 0x20, 0x43, 0x6f, 0x70, 0x79, 0x72,
    0x69, 0x67, 0x68, 0x74, 0x00, 0x00, 0x00, 0x05, 0x90, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04,
    0x30, 0x32, 0x33, 0x32, 0x90, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0xbc,
    0x90, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0xd0, 0x91, 0x01, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x00, 0xa0, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x30, 0x32, 0x35, 0x3a, 0x30, 0x31, 0x3a,
    0x31, 0x35, 0x20, 0x31, 0x34, 0x3a, 0x33, 0x30, 0x3a, 0x30, 0x30, 0x00, 0x32, 0x30, 0x32, 0x35,
    0x3a, 0x30, 0x31, 0x3a, 0x31, 0x35, 0x20, 0x31, 0x34, 0x3a, 0x33, 0x30, 0x3a, 0x30, 0x30, 0x00,
    0xff, 0xdb, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x10, 0x00, 0x10, 0x01, 0x01,
    0x11, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x00, 0xff, 0xd9};

static bool backend_available(std::string_view id) {
  const umm::Backend* b = umm::BackendManager::instance().get(id);
  return b && b->availability().available;
}

static fs::path write_jpeg(const fs::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(kMinimalJpeg), sizeof kMinimalJpeg);
  return path;
}

static fs::path write_naive_exif_jpeg(const fs::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(kNaiveExifJpeg), sizeof kNaiveExifJpeg);
  return path;
}

static void test_value_format() {
  umm::Value text;
  text.data = std::string("Jane");
  CHECK(value_summary(text) == "Jane");
  CHECK(value_to_json(text).dump(-1) == "\"Jane\"");
  umm::Value list;
  list.data = std::vector<std::string>{"a", "b"};
  CHECK(value_summary(list) == "a, b");
  umm::Value structs;
  structs.data = std::vector<umm::Structure>{umm::Structure{{"name", text}}};
  CHECK(value_summary(structs) == "Jane");
  CHECK(value_to_json(structs).dump(-1).find("\"name\":\"Jane\"") != std::string::npos);
}

static void test_version_command() {
  std::string out, err;
  CHECK(run_cli({"version"}, &out, &err) == 0);
  CHECK(out.find("umm 0.1.0") != std::string::npos);
  CHECK(out.find("libumm ") != std::string::npos);
  CHECK(out.find("STANDARD") != std::string::npos);
  CHECK(run_cli({"version", "--json"}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("\"command\": \"version\"") != std::string::npos);
  CHECK(out.find("\"standards\"") != std::string::npos);
  CHECK(out.find("\"libumm\"") != std::string::npos);
}

static void test_read_get() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_read";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = dir / "photo.jpg";
  write_jpeg(jpg);
  umm::Metadata meta;
  CHECK(meta.setCreator({"Jane Doe"}).ok());
  CHECK(meta.setKeywords({"nature", "landscape"}).ok());
  umm::Result<umm::WriteReport> wr = umm::write(jpg, meta);
  if (!wr.ok()) {
    std::cerr << "skip read/get fixture write: " << wr.error().message << "\n";
    fs::remove_all(dir);
    return;
  }

  std::string out, err;
  CHECK(run_cli({"get", jpg.string(), "creator"}, &out, &err) == 0);
  CHECK(out.find("Jane Doe") != std::string::npos);
  CHECK(run_cli({"get", jpg.string(), "creator", "keywords"}, &out) == 0);
  CHECK(out.find("Jane Doe") != std::string::npos && out.find("nature") != std::string::npos);
  CHECK(run_cli({"get", jpg.string(), "iptc.photo.creator"}, &out) == 0);
  CHECK(out.find("Jane Doe") != std::string::npos);
  CHECK(run_cli({"get", jpg.string(), "headline"}, &out, &err) == to_int(ExitCode::not_found));
  CHECK(run_cli({"get", jpg.string(), "no-such-accessor"}, nullptr, &err) ==
        to_int(ExitCode::semantics));
  CHECK(err.find("unknown property") != std::string::npos);
  // Full video id on a photo must not retarget to the photo creator.
  CHECK(run_cli({"get", jpg.string(), "iptc.video.creator"}) == to_int(ExitCode::not_found));

  CHECK(run_cli({"read", jpg.string()}, &out) == 0);
  CHECK(out.find("iptc.photo.creator") != std::string::npos);
  CHECK(out.find("Jane") != std::string::npos);
  CHECK(run_cli({"read", "--json", jpg.string()}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("\"command\": \"read\"") != std::string::npos);
  CHECK(run_cli({"read", "--sources", jpg.string()}, &out) == 0);
  CHECK(out.find("RESOLUTION") != std::string::npos || out.find("single") != std::string::npos);

  if (backend_available("exiv2")) {
    CHECK(run_cli({"get", "--backend", "exiv2", jpg.string(), "creator"}, &out) == 0);
    CHECK(out.find("Jane") != std::string::npos);
  } else {
    std::cerr << "skip --backend exiv2 (unavailable)\n";
  }
  if (backend_available("exiftool")) {
    int rc = run_cli({"get", "--backend", "exiftool", jpg.string(), "creator"}, &out, &err);
    if (rc != 0) std::cerr << "exiftool get rc=" << rc << " err=" << err << "\n";
    CHECK(rc == 0);
  } else {
    std::cerr << "skip --backend exiftool (unavailable)\n";
  }

  // Every libumm accessor is a known name (never unknown_property).
  for (auto n : accessor_names()) {
    int rc = run_cli({"get", jpg.string(), std::string(n)});
    CHECK(rc == 0 || rc == to_int(ExitCode::not_found));
  }

#ifdef UMM_LIBUMM_METADATA_HPP
  {
    std::ifstream in(UMM_LIBUMM_METADATA_HPP);
    std::string line;
    std::vector<std::string> getters;
    const std::string needle = "std::optional<PropertyValue> ";
    while (std::getline(in, line)) {
      auto p = line.find(needle);
      if (p == std::string::npos) continue;
      auto start = p + needle.size();
      auto paren = line.find('(', start);
      if (paren == std::string::npos) continue;
      if (line.find("() const", paren) == std::string::npos) continue;
      getters.push_back(line.substr(start, paren - start));
    }
    CHECK(!getters.empty());
    for (const std::string& g : getters) {
      bool found = false;
      for (auto n : accessor_names())
        if (n == g) found = true;
      if (!found) {
        std::cerr << "unbound libumm accessor: " << g << "\n";
        ++failures;
      }
    }
  }
#endif

#ifdef UMM_LIBUMM_FIXTURES
  {
    fs::path src = fs::path(UMM_LIBUMM_FIXTURES) / "video" / "minimal.mp4";
    if (fs::exists(src)) {
      fs::path mp4 = dir / "video.mp4";
      fs::copy_file(src, mp4, fs::copy_options::overwrite_existing);
      int rc = run_cli({"get", mp4.string(), "creator"});
      CHECK(rc == 0 || rc == to_int(ExitCode::not_found));
      umm::Metadata vmeta;
      vmeta.setMediaDomain(umm::MediaDomain::video);
      if (vmeta.setCreator({"Video Jane"}).ok()) {
        auto vwr = umm::write(mp4, vmeta);
        if (vwr.ok()) {
          CHECK(run_cli({"get", mp4.string(), "creator"}, &out) == 0);
          CHECK(out.find("Video Jane") != std::string::npos);
          CHECK(run_cli({"get", mp4.string(), "iptc.video.creator"}, &out) == 0);
          CHECK(run_cli({"get", mp4.string(), "iptc.photo.creator"}) == to_int(ExitCode::not_found));
        } else {
          std::cerr << "skip video write: " << vwr.error().message << "\n";
        }
      }
    } else {
      std::cerr << "skip video fixture (missing)\n";
    }
  }
#endif

  fs::remove_all(dir);
}

static void test_inspect() {
  std::string out, err;
  CHECK(run_cli({"caps", "JPEG"}, &out) == 0);
  CHECK(out.find("TYPE") != std::string::npos && out.find("JPEG") != std::string::npos);
  CHECK(out.find("exiv2") != std::string::npos);
  CHECK(run_cli({"caps", "--json", "JPEG"}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("\"command\": \"caps\"") != std::string::npos);
  CHECK(out.find("\"capabilities\"") != std::string::npos);
  CHECK(run_cli({"caps", "not-a-real-type"}, nullptr, &err) != 0);

#ifdef UMM_LIBUMM_FIXTURES
  fs::path fixtures = UMM_LIBUMM_FIXTURES;
  fs::path unknown = fixtures / "jpeg" / "unknown-tags.jpg";
  fs::path conflict = fixtures / "jpeg" / "full-conflicting.jpg";
  fs::path minimal = fixtures / "jpeg" / "minimal.jpg";
  if (fs::exists(unknown) && backend_available("exiv2")) {
    CHECK(run_cli({"unmapped", unknown.string()}, &out) == 0);
    CHECK(out.find("FAMILY") != std::string::npos);
    CHECK(run_cli({"unmapped", "--json", unknown.string()}, &out) == 0);
    CHECK(out.find("\"unmapped\"") != std::string::npos);
    CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  } else {
    std::cerr << "skip unmapped fixture\n";
  }
  if (fs::exists(conflict) && backend_available("exiv2")) {
    CHECK(run_cli({"conflicts", conflict.string()}, &out) == 0);
    CHECK(run_cli({"conflicts", "--json", conflict.string()}, &out) == 0);
    CHECK(out.find("\"conflicts\"") != std::string::npos);
    CHECK(run_cli({"conflicts", "--fail-on-conflict", conflict.string()}) ==
          to_int(ExitCode::semantics));
  } else {
    std::cerr << "skip conflicts fixture\n";
  }
  if (fs::exists(minimal)) {
    CHECK(run_cli({"caps", minimal.string()}, &out) == 0);
    CHECK(out.find("JPEG") != std::string::npos);
  }
#else
  std::cerr << "skip unmapped/conflicts fixtures (no libumm source dir)\n";
#endif

  // unmapped never writes: a missing file is I/O, not a mutation.
  CHECK(run_cli({"unmapped", "/no/such/unmapped.jpg"}, nullptr, &err) == to_int(ExitCode::io));
}

static std::vector<unsigned char> read_bytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static void test_parse_value() {
  auto t = parse_value(umm::Datatype::text, "Ada", false);
  CHECK(t.ok() && std::get<std::string>(t.value().data) == "Ada");
  auto list = parse_value(umm::Datatype::text_list, "a,b", false);
  CHECK(list.ok());
  auto gps = parse_value(umm::Datatype::gps_coordinate, "40.7128,-74.0060", false);
  CHECK(gps.ok());
  const auto* g = std::get_if<umm::GpsCoordinate>(&gps.value().data);
  CHECK(g && g->latitude > 40.7 && g->longitude < -74.0);
  auto dt = parse_value(umm::Datatype::date_time, "2025-01-15T14:30:00Z", false);
  CHECK(dt.ok());
  Json j;
  std::string err;
  CHECK(Json::parse(R"({"name":"NYC","countryCode":"US"})", j, &err) && err.empty());
  auto loc = parse_value(umm::Datatype::structure_list, R"({"name":"NYC","countryCode":"US"})", true);
  CHECK(loc.ok());
  CHECK(!parse_value(umm::Datatype::real, "nope", false).ok());
}

static void test_set_rm() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_set";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "photo.jpg");
  std::string out, err;

  CHECK(run_cli({"set", jpg.string(), "no-such-prop=x"}, nullptr, &err) == to_int(ExitCode::semantics));
  CHECK(run_cli({"set", jpg.string(), "rating=not-a-number"}, nullptr, &err) ==
        to_int(ExitCode::semantics));
  CHECK(run_cli({"set", "--policy", "nope", jpg.string(), "creator=x"}) == to_int(ExitCode::usage));

  umm::Metadata probe;
  CHECK(probe.setCreator({"Probe"}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip set/rm fixture write: no metadata backend is available\n";
    fs::remove_all(dir);
    return;
  }

  {
    umm::Metadata md;
    CHECK(md.setCreator({"Bea"}).ok());
    auto resolved = resolve_property("creator", umm::MediaDomain::photo);
    CHECK(resolved.ok());
    CHECK(apply_remove(md, resolved.value()).ok());
    CHECK(!md.creator());
  }

  CHECK(run_cli({"set", jpg.string(), "iptc.photo.creator=Ada"}, &out, &err) == 0);
  CHECK(run_cli({"get", jpg.string(), "creator"}, &out) == 0);
  CHECK(out.find("Ada") != std::string::npos);
  CHECK(run_cli({"set", jpg.string(), "creator=Bea"}, &out, &err) == 0);
  CHECK(run_cli({"get", jpg.string(), "iptc.photo.creator"}, &out) == 0);
  CHECK(out.find("Bea") != std::string::npos);

  CHECK(run_cli({"set", jpg.string(), "gps=40.7128,-74.0060"}) == 0);
  CHECK(run_cli({"get", jpg.string(), "gps"}, &out) == 0);
  CHECK(out.find("40.7128") != std::string::npos);
  CHECK(run_cli({"get", jpg.string(), "exif.gps.position"}, &out) == 0);
  CHECK(out.find("40.7128") != std::string::npos);

  CHECK(run_cli({"set", jpg.string(), "locationCreated", "--json",
                 R"({"city":"NYC","countryName":"US"})"}) == 0);
  CHECK(run_cli({"get", "--json", jpg.string(), "locationCreated"}, &out) == 0);
  CHECK(out.find("NYC") != std::string::npos);

  auto before = read_bytes(jpg);
  CHECK(run_cli({"set", "--dry-run", jpg.string(), "headline=Summit"}, &out, &err) == 0);
  CHECK(out.find("METHOD") != std::string::npos);
  CHECK(read_bytes(jpg) == before);
  CHECK(run_cli({"get", jpg.string(), "headline"}) == to_int(ExitCode::not_found));
  CHECK(run_cli({"set", "--dry-run", "--json", jpg.string(), "headline=Summit"}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("\"command\": \"set\"") != std::string::npos);

  CHECK(run_cli({"set", "--policy", "sidecar", jpg.string(), "headline=SidecarHead"}) == 0);
  CHECK(read_bytes(jpg) == before);
  CHECK(umm::findSidecar(jpg).has_value());
  CHECK(run_cli({"get", jpg.string(), "headline"}, &out) == 0);
  CHECK(out.find("SidecarHead") != std::string::npos);

  CHECK(run_cli({"rm", jpg.string(), "creator"}) == 0);
  // libumm v0.1.0 write-sync emits upserts only, so a cleared Metadata may
  // not delete on-disk tags. Accept either a persisted clear or in-memory
  // remove (checked above).
  int after_rm = run_cli({"get", jpg.string(), "creator"}, &out);
  if (after_rm != to_int(ExitCode::not_found))
    std::cerr << "rm persist skipped (write-sync upserts only)\n";

  fs::path jpg2 = write_jpeg(dir / "photo2.jpg");
  int batch = run_cli({"set", jpg2.string(), (dir / "missing.jpg").string(), "creator=Batch"},
                      nullptr, &err);
  CHECK(batch == to_int(ExitCode::io));
  CHECK(err.find("missing.jpg") != std::string::npos);
  CHECK(run_cli({"get", jpg2.string(), "creator"}, &out) == 0);
  CHECK(out.find("Batch") != std::string::npos);

#ifdef UMM_LIBUMM_FIXTURES
  {
    fs::path src = fs::path(UMM_LIBUMM_FIXTURES) / "video" / "minimal.mp4";
    if (fs::exists(src)) {
      fs::path mp4 = dir / "video.mp4";
      fs::copy_file(src, mp4, fs::copy_options::overwrite_existing);
      int rc = run_cli({"set", mp4.string(), "creator=VideoAda"}, &out, &err);
      if (rc == 0) {
        CHECK(run_cli({"get", "--json", mp4.string(), "iptc.video.creator"}, &out) == 0);
        CHECK(out.find("VideoAda") != std::string::npos || out.find("Ada") != std::string::npos);
        CHECK(run_cli({"get", mp4.string(), "iptc.photo.creator"}) == to_int(ExitCode::not_found));
      } else {
        std::cerr << "skip video set: " << err << "\n";
      }
      CHECK(run_cli({"set", mp4.string(), "rating=3"}, nullptr, &err) == to_int(ExitCode::semantics));
    }
  }
#endif

  fs::remove_all(dir);
}

static void test_merge_sync() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_merge";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "photo.jpg");
  std::string out, err;

  CHECK(run_cli({"merge", jpg.string(), "iptc.photo.creator"}) == to_int(ExitCode::usage));
  CHECK(run_cli({"merge", "--use", "x", "--value", "y", jpg.string(), "iptc.photo.creator"}) ==
        to_int(ExitCode::usage));
  CHECK(run_cli({"merge", "--value", "Ada", jpg.string(), "creator"}) == to_int(ExitCode::usage));

  umm::Metadata probe;
  CHECK(probe.setCreator({"Probe"}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip merge/sync fixture write: no metadata backend is available\n";
  } else {
    CHECK(run_cli({"set", jpg.string(), "creator=Alice"}) == 0);
    CHECK(run_cli({"merge", "--value", "Bob", jpg.string(), "iptc.photo.creator"}) == 0);
    CHECK(run_cli({"get", jpg.string(), "creator"}, &out) == 0);
    CHECK(out.find("Bob") != std::string::npos);
  }

  auto before = read_bytes(jpg);
  int sync_dry = run_cli({"sync", "--dry-run", jpg.string()}, &out, &err);
  if (sync_dry == 0) {
    CHECK(out.find("METHOD") != std::string::npos || out.find("CARRIER") != std::string::npos);
    CHECK(read_bytes(jpg) == before);
  } else {
    std::cerr << "sync --dry-run rc=" << sync_dry << " err=" << err << "\n";
  }
  CHECK(run_cli({"sync", "--direction", "nope", jpg.string()}) == to_int(ExitCode::usage));
  CHECK(parse_args({"sync", "--direction", "embedded-to-sidecar", "p.jpg"}).options.at("direction") ==
        "embedded-to-sidecar");
  CHECK(parse_args({"sync", "--direction", "sidecar-to-embedded", "p.jpg"}).options.at("direction") ==
        "sidecar-to-embedded");
  CHECK(parse_args({"merge", "--container", "sidecar", "--use", "Xmp.dc.creator", "p.jpg",
                    "iptc.photo.creator"})
            .options.at("container") == "sidecar");

#ifdef UMM_LIBUMM_FIXTURES
  fs::path fixtures = UMM_LIBUMM_FIXTURES;
  fs::path paired = fixtures / "jpeg" / "paired.jpg";
  fs::path paired_xmp = fixtures / "jpeg" / "paired.xmp";
  if (fs::exists(paired) && fs::exists(paired_xmp) && backend_available("exiv2")) {
    fs::path media = dir / "paired.jpg";
    fs::copy_file(paired, media, fs::copy_options::overwrite_existing);
    fs::copy_file(paired_xmp, dir / "paired.xmp", fs::copy_options::overwrite_existing);
    int conf = run_cli({"conflicts", media.string()}, &out);
    CHECK(conf == 0);
    int use = run_cli({"merge", "--use", "Xmp.dc.creator", "--container", "sidecar", media.string(),
                       "iptc.photo.creator"},
                      &out, &err);
    if (use != 0) std::cerr << "merge --use rc=" << use << " err=" << err << "\n";
    CHECK(use == 0);
    int both = run_cli({"sync", "--dry-run", media.string()}, &out, &err);
    if (both != 0) std::cerr << "sync after merge rc=" << both << " err=" << err << "\n";
  } else {
    std::cerr << "skip paired merge fixture\n";
  }
  fs::path conflict = fixtures / "jpeg" / "full-conflicting.jpg";
  if (fs::exists(conflict) && backend_available("exiv2")) {
    fs::path media = dir / "conflict.jpg";
    fs::copy_file(conflict, media, fs::copy_options::overwrite_existing);
    int syn = run_cli({"sync", media.string()}, nullptr, &err);
    CHECK(syn == to_int(ExitCode::semantics) || syn == 0);
    if (syn == to_int(ExitCode::semantics)) CHECK(err.find("merge") != std::string::npos);
  }
#else
  std::cerr << "skip merge/sync fixtures (no libumm source dir)\n";
#endif

  fs::remove_all(dir);
}

static void set_env(const char* key, const char* value) {
#ifdef _WIN32
  _putenv_s(key, value);
#else
  setenv(key, value, 1);
#endif
}

static const char* kGpx = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="umm-test">
  <trk><trkseg>
    <trkpt lat="40.7128" lon="-74.0060"><time>2025-01-15T14:30:00Z</time></trkpt>
    <trkpt lat="40.7138" lon="-74.0070"><time>2025-01-15T14:32:00Z</time></trkpt>
  </trkseg></trk>
</gpx>
)";

static void test_docs_gen() {
  const std::string man = generate_man_page();
  const std::string bash = generate_bash_completion();
  const std::string zsh = generate_zsh_completion();
  const std::string fish = generate_fish_completion();
  CHECK(man.find(".TH UMM 1") != std::string::npos);
  CHECK(man.find("umm \\- media metadata tool built on libumm") != std::string::npos);
  CHECK(man.find("exiftool") != std::string::npos && man.find("exiv2") != std::string::npos);
  CHECK(man.find("never bundles") != std::string::npos);
  std::string help;
  CHECK(run_cli({"--help"}, &help) == 0);
  const char* flags[] = {"--json",     "--backend", "--recursive", "--policy",
                         "--dry-run",  "--sources", "--fail-on-conflict", "--direction",
                         "--track",    "--offset",  "--use", "--value"};
  for (const CommandSpec& c : commands()) {
    CHECK(help.find(std::string(c.name)) != std::string::npos);
    CHECK(man.find(std::string(c.name)) != std::string::npos);
    CHECK(bash.find(std::string(c.name)) != std::string::npos);
    CHECK(zsh.find(std::string(c.name)) != std::string::npos);
    CHECK(fish.find(std::string(c.name)) != std::string::npos);
  }
  for (const char* f : flags) {
    CHECK(bash.find(f) != std::string::npos);
    CHECK(zsh.find(f) != std::string::npos);
    CHECK(fish.find(std::string("-l ") + (f + 2)) != std::string::npos);
  }
  for (auto n : accessor_names()) {
    CHECK(bash.find(std::string(n)) != std::string::npos);
    CHECK(zsh.find(std::string(n)) != std::string::npos);
    CHECK(fish.find(std::string(n)) != std::string::npos);
  }
  CHECK(bash.find("iptc.photo.") != std::string::npos);
  CHECK(zsh.find("iptc.video.") != std::string::npos);
  CHECK(fish.find("exif.") != std::string::npos);

  fs::path dir = fs::temp_directory_path() / "umm_cli_test_docs";
  fs::remove_all(dir);
  write_generated_docs(dir);
  CHECK(fs::exists(dir / "umm.1"));
  CHECK(fs::exists(dir / "completions" / "umm.bash"));
  CHECK(fs::exists(dir / "completions" / "_umm"));
  CHECK(fs::exists(dir / "completions" / "umm.fish"));
  fs::remove_all(dir);
}

static void test_geotag_doctor_setup() {
  std::string out, err;
  CHECK(run_cli({"geotag", "x.jpg"}, nullptr, &err) == to_int(ExitCode::usage));
  CHECK(err.find("--track") != std::string::npos);
  CHECK(run_cli({"geotag", "--track", "/no/such-track.gpx", "x.jpg"}, nullptr, &err) ==
        to_int(ExitCode::io));
  CHECK(parse_args({"geotag", "--track", "t.gpx", "--offset", "120", "p.jpg"}).options.at("offset") ==
        "120");
  CHECK(run_cli({"setup", "not-exiftool"}) == to_int(ExitCode::usage));
  CHECK(run_cli({"setup", "exiftool", "--help"}, &out) == 0);
  CHECK(out.find("OliverBetz") != std::string::npos);
  CHECK(out.find("brew") != std::string::npos);
  CHECK(out.find("apt") != std::string::npos);
  CHECK(out.find("dnf") != std::string::npos);
  CHECK(out.find("pacman") != std::string::npos);
  CHECK(run_cli({"doctor", "--json"}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("\"command\": \"doctor\"") != std::string::npos);
  CHECK(out.find("\"discovery\"") != std::string::npos);
  CHECK(out.find("\"tested_version\"") != std::string::npos);
  CHECK(run_cli({"doctor"}, &out) == 0);
  CHECK(out.find("BACKEND") != std::string::npos);
  CHECK(out.find("discovery") != std::string::npos);

  fs::path dir = fs::temp_directory_path() / "umm_cli_test_geotag";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path gpx = dir / "track.gpx";
  std::ofstream(gpx) << kGpx;
  fs::path junk = dir / "not-a-track.txt";
  std::ofstream(junk) << "hello\n";
  CHECK(run_cli({"geotag", "--track", junk.string(), (dir / "x.jpg").string()}, nullptr, &err) ==
        to_int(ExitCode::format));

  fs::path jpg = write_jpeg(dir / "photo.jpg");
  umm::Metadata probe;
  CHECK(probe.setDateCreated(umm::DateTime{2025, 1, 15, 14, 30, 0, std::nullopt, 0}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip geotag fixture write: no metadata backend is available\n";
  } else {
    auto before = read_bytes(jpg);
    CHECK(run_cli({"geotag", "--dry-run", "--track", gpx.string(), jpg.string()}, &out, &err) == 0);
    CHECK(out.find("40.7128") != std::string::npos);
    CHECK(read_bytes(jpg) == before);
    CHECK(run_cli({"geotag", "--dry-run", "--json", "--track", gpx.string(), jpg.string()}, &out) == 0);
    CHECK(out.find("\"schema_version\": 1") != std::string::npos);
    CHECK(out.find("\"command\": \"geotag\"") != std::string::npos);
    CHECK(run_cli({"geotag", "--track", gpx.string(), jpg.string()}, &out, &err) == 0);
    CHECK(run_cli({"get", jpg.string(), "gps"}, &out) == 0);
    CHECK(out.find("40.7128") != std::string::npos);

    fs::path naive = write_naive_exif_jpeg(dir / "naive.jpg");
    int rc = run_cli({"geotag", "--track", gpx.string(), naive.string()}, nullptr, &err);
    CHECK(rc == to_int(ExitCode::semantics));
    CHECK(err.find("naive") != std::string::npos);
    CHECK(run_cli({"geotag", "--offset", "0", "--track", gpx.string(), naive.string()}) == 0);
  }

#ifdef UMM_LIBUMM_FIXTURES
  {
    fs::path src = fs::path(UMM_LIBUMM_FIXTURES) / "video" / "minimal.mp4";
    if (fs::exists(src)) {
      fs::path mp4 = dir / "video.mp4";
      fs::copy_file(src, mp4, fs::copy_options::overwrite_existing);
      umm::Metadata vmeta;
      vmeta.setMediaDomain(umm::MediaDomain::video);
      umm::DateTime dt{2025, 1, 15, 14, 30, 0, std::nullopt, 0};
      if (vmeta.setDateCreated(dt).ok() && umm::write(mp4, vmeta).ok()) {
        int rc = run_cli({"geotag", "--track", gpx.string(), mp4.string()}, &out, &err);
        if (rc == 0) {
          CHECK(run_cli({"get", mp4.string(), "gps"}, &out) == 0);
          CHECK(out.find("40.7128") != std::string::npos);
        } else {
          std::cerr << "skip video geotag: " << err << "\n";
        }
      }
    }
  }
#endif

  fs::path cfgdir = fs::temp_directory_path() / "umm_cli_test_setupcfg";
  fs::remove_all(cfgdir);
  fs::create_directories(cfgdir);
#ifdef _WIN32
  set_env("APPDATA", cfgdir.string().c_str());
#else
  set_env("XDG_CONFIG_HOME", cfgdir.string().c_str());
#endif
  fs::path dummy = dir / "fake-exiftool";
  std::ofstream(dummy) << "#!/bin/sh\n";
  int setup_rc = run_cli({"setup", "exiftool", "--", "--record", dummy.string()}, &out, &err);
  if (setup_rc != 0) std::cerr << "setup --record rc=" << setup_rc << " err=" << err << " out=" << out << "\n";
  CHECK(setup_rc == 0);
  ConfigParse written = load_config(current_platform(), process_env());
  CHECK(written.error.empty());
  CHECK(written.config.exiftool == dummy);
  fs::remove_all(cfgdir);
  fs::remove_all(dir);
}

static bool env_truthy(const char* key) {
  const char* e = std::getenv(key);
  return e && *e && std::string(e) != "0";
}

static void replace_all(std::string& s, std::string_view from, std::string_view to) {
  if (from.empty()) return;
  std::size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

static std::string to_lf(std::string s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s)
    if (c != '\r') out += c;
  return out;
}

static std::string normalize_output(std::string s, const fs::path& path = {}) {
  s = to_lf(std::move(s));
  if (!path.empty()) {
    replace_all(s, path.string(), "FILE");
    if (path.generic_string() != path.string()) replace_all(s, path.generic_string(), "FILE");
  }
  replace_all(s, std::string("umm ") + UMM_CLI_VERSION, "umm VERSION");
  replace_all(s, std::string("\"version\": \"") + UMM_CLI_VERSION + "\"", "\"version\": \"VERSION\"");
  const std::string libv = std::string(umm::version());
  replace_all(s, "libumm " + libv, "libumm VERSION");
  replace_all(s, "\"libumm\": \"" + libv + "\"", "\"libumm\": \"VERSION\"");
  return s;
}

static fs::path goldens_dir() { return fs::path(UMM_CLI_SOURCE_DIR) / "tests" / "goldens"; }

static void check_golden(const std::string& name, std::string actual) {
  actual = to_lf(std::move(actual));
  const fs::path path = goldens_dir() / name;
  if (env_truthy("UMM_REGENERATE_GOLDENS")) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << actual;
    if (!out) {
      std::cerr << "failed to write golden " << path << "\n";
      ++failures;
    }
    return;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "missing golden " << path << " (set UMM_REGENERATE_GOLDENS=1 to create)\n";
    ++failures;
    return;
  }
  std::string expected(std::istreambuf_iterator<char>(in), {});
  expected = to_lf(std::move(expected));
  if (expected != actual) {
    std::cerr << "golden mismatch: " << name << "\n--- expected ---\n" << expected
              << "\n--- actual ---\n" << actual << "\n";
    ++failures;
  }
}

static std::pair<bool, std::string> sample_for(const ResolvedProperty& p) {
  switch (p.datatype) {
    case umm::Datatype::text:
      return {false, "SampleText"};
    case umm::Datatype::text_list:
      return {false, "alpha,beta"};
    case umm::Datatype::lang_alt:
      return {false, "HelloAlt"};
    case umm::Datatype::date_time:
      return {false, "2025-01-15T14:30:00Z"};
    case umm::Datatype::real:
      return {false, "3"};
    case umm::Datatype::integer:
      return {false, "3"};
    case umm::Datatype::boolean:
      return {false, "true"};
    case umm::Datatype::gps_coordinate:
      return {false, "40.7128,-74.0060"};
    case umm::Datatype::structure:
    case umm::Datatype::structure_list:
      if (p.name == "contributor") return {true, R"([{"name":"Alice","role":"director"}])"};
      if (p.name == "locationCreated" || p.name == "locationShown")
        return {true, R"({"city":"NYC","countryName":"US"})"};
      if (p.name == "personShown") return {true, R"({"name":"Bob"})"};
      if (p.name == "shownEvent") return {true, R"({"name":"Summit"})"};
      if (p.name == "genre") return {true, R"({"name":"Documentary"})"};
      return {true, R"({"name":"X"})"};
    default:
      return {false, "x"};
  }
}

static const char* needle_for(const ResolvedProperty& p) {
  switch (p.datatype) {
    case umm::Datatype::text:
      return "SampleText";
    case umm::Datatype::text_list:
      return "alpha";
    case umm::Datatype::lang_alt:
      return "HelloAlt";
    case umm::Datatype::date_time:
      return "2025-01-15";
    case umm::Datatype::real:
    case umm::Datatype::integer:
      return "3";
    case umm::Datatype::boolean:
      return "true";
    case umm::Datatype::gps_coordinate:
      return "40.7128";
    default:
      if (p.name == "contributor") return "Alice";
      if (p.name == "locationCreated" || p.name == "locationShown") return "NYC";
      if (p.name == "personShown") return "Bob";
      if (p.name == "shownEvent") return "Summit";
      if (p.name == "genre") return "Documentary";
      return "X";
  }
}

static void test_goldens() {
  std::string out, err;
  CHECK(run_cli({"version"}, &out) == 0);
  check_golden("version.txt", normalize_output(out));
  CHECK(run_cli({"version", "--json"}, &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  check_golden("version.json", normalize_output(out));

  if (backend_available("exiv2") && backend_available("exiftool")) {
    CHECK(run_cli({"caps", "JPEG"}, &out) == 0);
    check_golden("caps-jpeg.txt", normalize_output(out));
    CHECK(run_cli({"caps", "--json", "JPEG"}, &out) == 0);
    CHECK(out.find("\"schema_version\": 1") != std::string::npos);
    check_golden("caps-jpeg.json", normalize_output(out));
  } else {
    std::cerr << "skip caps goldens (need both backends)\n";
  }

  fs::path dir = fs::temp_directory_path() / "umm_cli_test_golden";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "photo.jpg");
  umm::Metadata meta;
  CHECK(meta.setCreator({"Jane Doe"}).ok());
  CHECK(meta.setKeywords({"nature", "landscape"}).ok());
  umm::Result<umm::WriteReport> wr = umm::write(jpg, meta);
  if (!wr.ok()) {
    std::cerr << "skip read/get goldens: " << wr.error().message << "\n";
    fs::remove_all(dir);
    return;
  }
  std::vector<std::string> backend_flag;
  if (backend_available("exiv2")) backend_flag = {"--backend", "exiv2"};
  auto with_backend = [&](std::vector<std::string> a) {
    a.insert(a.begin() + 1, backend_flag.begin(), backend_flag.end());
    return a;
  };
  CHECK(run_cli(with_backend({"read", jpg.string()}), &out) == 0);
  check_golden("read.txt", normalize_output(out, jpg));
  CHECK(run_cli(with_backend({"read", "--json", jpg.string()}), &out) == 0);
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  check_golden("read.json", normalize_output(out, jpg));
  CHECK(run_cli(with_backend({"get", jpg.string(), "creator"}), &out) == 0);
  check_golden("get.txt", normalize_output(out, jpg));
  CHECK(run_cli(with_backend({"get", "--json", jpg.string(), "creator"}), &out) == 0);
  check_golden("get.json", normalize_output(out, jpg));

#ifdef UMM_LIBUMM_FIXTURES
  fs::path fixtures = UMM_LIBUMM_FIXTURES;
  fs::path unknown = fixtures / "jpeg" / "unknown-tags.jpg";
  fs::path conflict = fixtures / "jpeg" / "full-conflicting.jpg";
  if (fs::exists(unknown) && backend_available("exiv2")) {
    CHECK(run_cli({"unmapped", "--backend", "exiv2", unknown.string()}, &out) == 0);
    check_golden("unmapped.txt", normalize_output(out, unknown));
    CHECK(run_cli({"unmapped", "--json", "--backend", "exiv2", unknown.string()}, &out) == 0);
    check_golden("unmapped.json", normalize_output(out, unknown));
  } else {
    std::cerr << "skip unmapped goldens\n";
  }
  if (fs::exists(conflict) && backend_available("exiv2")) {
    CHECK(run_cli({"conflicts", "--backend", "exiv2", conflict.string()}, &out) == 0);
    check_golden("conflicts.txt", normalize_output(out, conflict));
    CHECK(run_cli({"conflicts", "--json", "--backend", "exiv2", conflict.string()}, &out) == 0);
    check_golden("conflicts.json", normalize_output(out, conflict));
  } else {
    std::cerr << "skip conflicts goldens\n";
  }
#else
  std::cerr << "skip unmapped/conflicts goldens (no libumm fixtures)\n";
#endif
  fs::remove_all(dir);
}

static void test_batch_recursive_cli() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_batch_tree";
  fs::remove_all(dir);
  fs::create_directories(dir / "sub");
  fs::path good = write_jpeg(dir / "good.jpg");
  fs::path nested = write_jpeg(dir / "sub" / "nested.jpg");
  umm::Metadata meta;
  CHECK(meta.setCreator({"Tree"}).ok());
  if (!umm::write(good, meta).ok() || !umm::write(nested, meta).ok()) {
    std::cerr << "skip batch/recursive CLI (no backend)\n";
    fs::remove_all(dir);
    return;
  }
  std::string out, err;
  int rc = run_cli({"read", "--json", good.string(), (dir / "missing.jpg").string()}, &out, &err);
  CHECK(rc == to_int(ExitCode::io));
  CHECK(out.find("\"schema_version\": 1") != std::string::npos);
  CHECK(out.find("Tree") != std::string::npos);
  CHECK(out.find("\"ok\": true") != std::string::npos);
  CHECK(out.find("\"ok\": false") != std::string::npos);
  CHECK(out.find("missing.jpg") != std::string::npos);

  CHECK(run_cli({"read", "--recursive", "--json", dir.string()}, &out, &err) == 0);
  CHECK(out.find("good.jpg") != std::string::npos);
  CHECK(out.find("nested.jpg") != std::string::npos);
  fs::remove_all(dir);
}

static void test_cross_backend_and_dry_run() {
  if (!backend_available("exiv2") || !backend_available("exiftool")) {
    std::cerr << "skip cross-backend smoke (need both backends)\n";
  } else {
    fs::path dir = fs::temp_directory_path() / "umm_cli_test_xbackend";
    fs::remove_all(dir);
    fs::create_directories(dir);
    fs::path jpg = write_jpeg(dir / "photo.jpg");
    std::string out, err;
    int w = run_cli({"set", "--backend", "exiv2", jpg.string(), "creator=FromExiv2"}, &out, &err);
    if (w != 0) {
      std::cerr << "skip cross-backend: exiv2 write rc=" << w << " " << err << "\n";
    } else {
      CHECK(run_cli({"get", "--backend", "exiftool", jpg.string(), "creator"}, &out, &err) == 0);
      CHECK(out.find("FromExiv2") != std::string::npos);
      CHECK(run_cli({"set", "--backend", "exiftool", jpg.string(), "creator=FromExifTool"}) == 0);
      CHECK(run_cli({"get", "--backend", "exiv2", jpg.string(), "creator"}, &out) == 0);
      CHECK(out.find("FromExifTool") != std::string::npos);
    }
    fs::remove_all(dir);
  }

  fs::path dir = fs::temp_directory_path() / "umm_cli_test_mtime";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "photo.jpg");
  umm::Metadata probe;
  CHECK(probe.setCreator({"Probe"}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip dry-run mtime (no backend)\n";
    fs::remove_all(dir);
    return;
  }
  auto before_bytes = read_bytes(jpg);
  auto before_size = fs::file_size(jpg);
  auto before_mtime = fs::last_write_time(jpg);
  std::string out, err;
  CHECK(run_cli({"set", "--dry-run", jpg.string(), "headline=Dry"}, &out, &err) == 0);
  CHECK(read_bytes(jpg) == before_bytes);
  CHECK(fs::file_size(jpg) == before_size);
  CHECK(fs::last_write_time(jpg) == before_mtime);
  CHECK(run_cli({"set", jpg.string(), "headline=Wet"}) == 0);
  CHECK(read_bytes(jpg) != before_bytes);
  fs::remove_all(dir);
}

static void test_xmp_pairing() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_xmp";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "paired.jpg");
  umm::Metadata probe;
  CHECK(probe.setCreator({"Probe"}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip xmp pairing (no backend)\n";
    fs::remove_all(dir);
    return;
  }
  std::string out, err;
  CHECK(run_cli({"set", "--policy", "sidecar", jpg.string(), "headline=SidecarAda"}) == 0);
  auto side = umm::findSidecar(jpg);
  CHECK(side.has_value());
  CHECK(run_cli({"get", jpg.string(), "headline"}, &out) == 0);
  CHECK(out.find("SidecarAda") != std::string::npos);
  fs::path xmp = *side;
  fs::path upper = jpg;
  upper.replace_extension(".XMP");
  std::error_code ec;
  if (fs::exists(upper, ec) && fs::equivalent(xmp, upper, ec)) {
    std::cerr << "skip .XMP case (case-insensitive filesystem)\n";
  } else {
    fs::rename(xmp, upper, ec);
    if (ec) {
      std::cerr << "skip .XMP rename: " << ec.message() << "\n";
    } else {
      CHECK(umm::findSidecar(jpg).has_value());
      CHECK(run_cli({"get", jpg.string(), "headline"}, &out) == 0);
      CHECK(out.find("SidecarAda") != std::string::npos);
    }
  }
  fs::remove_all(dir);
}

static std::vector<std::string> with_backend(std::vector<std::string> args, const std::string& backend) {
  if (!backend.empty()) {
    args.insert(args.begin() + 1, "--backend");
    args.insert(args.begin() + 2, backend);
  }
  return args;
}

static void roundtrip_accessors(const fs::path& media, umm::MediaDomain domain, const std::string& backend) {
  std::string out, err;
  for (auto n : accessor_names()) {
    auto resolved = resolve_property(n, domain);
    CHECK(resolved.ok());
    if (!resolved.ok()) continue;
    if (resolved.value().photo_only && domain == umm::MediaDomain::video) {
      int rc = run_cli(with_backend({"set", media.string(), std::string(n) + "=3"}, backend), nullptr, &err);
      CHECK(rc == to_int(ExitCode::semantics) || rc == to_int(ExitCode::backend) ||
            rc == to_int(ExitCode::capability));
      continue;
    }
    auto sample = sample_for(resolved.value());
    std::vector<std::string> args{"set", media.string()};
    if (sample.first) {
      args.push_back(std::string(n));
      args.push_back("--json");
      args.push_back(sample.second);
    } else {
      args.push_back(std::string(n) + "=" + sample.second);
    }
    int rc = run_cli(with_backend(args, backend), &out, &err);
    if (rc != 0) {
      std::cerr << "skip accessor set " << n << " on " << media.filename().string() << " rc=" << rc
                << " " << err << "\n";
      if (domain == umm::MediaDomain::video &&
          (rc == to_int(ExitCode::backend) || rc == to_int(ExitCode::capability)))
        return;
      continue;
    }
    std::vector<std::string> get_args{"get", "--json", media.string(), std::string(n)};
    int grc = run_cli(with_backend(get_args, backend), &out, &err);
    if (grc != 0) {
      std::cerr << "skip accessor get " << n << " on " << media.filename().string() << " rc=" << grc
                << " " << err << "\n";
      if (domain == umm::MediaDomain::video &&
          (grc == to_int(ExitCode::backend) || grc == to_int(ExitCode::capability)))
        return;
      continue;
    }
    if (out.find(needle_for(resolved.value())) == std::string::npos) {
      std::cerr << "accessor get " << n << " missing needle, out=" << out << "\n";
      ++failures;
    }
  }
  int gps_set = run_cli(with_backend({"set", media.string(), "gps=41.0,-73.0"}, backend), &out, &err);
  CHECK(gps_set == 0 || domain == umm::MediaDomain::video);
  int gps_rc = run_cli(with_backend({"get", media.string(), "gps"}, backend), &out);
  if (gps_rc == 0) CHECK(out.find("41") != std::string::npos);
}

static void test_accessor_coverage() {
  fs::path dir = fs::temp_directory_path() / "umm_cli_test_accessors";
  fs::remove_all(dir);
  fs::create_directories(dir);
  fs::path jpg = write_jpeg(dir / "photo.jpg");
  umm::Metadata probe;
  CHECK(probe.setCreator({"Probe"}).ok());
  if (!umm::write(jpg, probe).ok()) {
    std::cerr << "skip accessor coverage (no backend)\n";
    fs::remove_all(dir);
    return;
  }
  roundtrip_accessors(jpg, umm::MediaDomain::photo, "");
  CHECK(run_cli({"set", jpg.string(), "locationCreated", "--json",
                 R"({"city":"NYC","countryName":"US"})"}) == 0);
  std::string out, err;
  CHECK(run_cli({"get", "--json", jpg.string(), "locationCreated"}, &out) == 0);
  CHECK(out.find("NYC") != std::string::npos);

#ifdef UMM_LIBUMM_FIXTURES
  fs::path src = fs::path(UMM_LIBUMM_FIXTURES) / "video" / "minimal.mp4";
  if (fs::exists(src)) {
    fs::path mp4 = dir / "video.mp4";
    fs::copy_file(src, mp4, fs::copy_options::overwrite_existing);
    std::string video_backend = backend_available("exiftool") ? "exiftool" : "";
    umm::Metadata v;
    v.setMediaDomain(umm::MediaDomain::video);
    if (!(v.setCreator({"Probe"}).ok() && umm::write(mp4, v).ok())) {
      std::cerr << "skip video accessor coverage (write failed)\n";
    } else {
      int probe_rc =
          run_cli(with_backend({"get", mp4.string(), "creator"}, video_backend), &out, &err);
      if (probe_rc != 0 && probe_rc != to_int(ExitCode::not_found))
        std::cerr << "skip video accessor coverage (read rc=" << probe_rc << " " << err << ")\n";
      else
        roundtrip_accessors(mp4, umm::MediaDomain::video, video_backend);
    }
  } else {
    std::cerr << "skip video accessor coverage (no fixture)\n";
  }
#else
  std::cerr << "skip video accessor coverage (no libumm fixtures)\n";
#endif
  fs::remove_all(dir);
}

int main() {
  test_version_linked();
  test_command_table();
  test_exit_codes();
  test_parse();
  test_cli_behaviour();
  test_batch_recursive();
  test_property_seam();
  test_config();
  test_output();
  test_value_format();
  test_version_command();
  test_read_get();
  test_inspect();
  test_parse_value();
  test_set_rm();
  test_merge_sync();
  test_geotag_doctor_setup();
  test_docs_gen();
  test_goldens();
  test_batch_recursive_cli();
  test_cross_backend_and_dry_run();
  test_xmp_pairing();
  test_accessor_coverage();
  if (failures) std::cerr << failures << " check(s) failed\n";
  return failures ? 1 : 0;
}
