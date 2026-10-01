// Small self-contained CLI tests (no test framework dependency).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "args.hpp"
#include "batch.hpp"
#include "cli.hpp"
#include "command_table.hpp"
#include "config.hpp"
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

static bool backend_available(std::string_view id) {
  const umm::Backend* b = umm::BackendManager::instance().get(id);
  return b && b->availability().available;
}

static fs::path write_jpeg(const fs::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(kMinimalJpeg), sizeof kMinimalJpeg);
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
  CHECK(value_summary(structs) == "1 entry");
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
  if (failures) std::cerr << failures << " check(s) failed\n";
  return failures ? 1 : 0;
}
