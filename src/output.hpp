// Output formatters: human table (default) and JSON with schema_version.
// Schema: docs/implementation/json-schema.md.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace umm_cli {

constexpr int kSchemaVersion = 1;

// Minimal JSON value (no third-party dependency).
class Json {
 public:
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;  // insertion order

  Json() = default;  // null
  Json(bool v) : v_(v) {}
  Json(int v) : v_(static_cast<double>(v)) {}
  Json(double v) : v_(v) {}
  Json(const char* v) : v_(std::string(v)) {}
  Json(std::string v) : v_(std::move(v)) {}
  Json(Array v) : v_(std::move(v)) {}
  Json(Object v) : v_(std::move(v)) {}

  // Compact when indent < 0.
  std::string dump(int indent = 2) const;

 private:
  void write(std::string& out, int indent, int depth) const;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v_{nullptr};
};

std::string json_escape(const std::string& s);

// One property row. `value` is the full JSON form (objects/arrays for
// structs, concept §2.5); `summary` is the compact human form.
struct PropertyRow {
  std::string id;
  Json value;
  std::string summary;
  std::vector<std::string> extra;  // extra human columns (e.g. source, state)
  Json::Object json_extra;         // merged into the JSON property object
};

struct FileReport {
  std::string path;
  bool ok{true};
  std::string error;  // when !ok
  std::vector<PropertyRow> properties;
  Json::Object json_extra;  // merged into the JSON file object (session 07+)
};

// Human table: "id  value  [extra...]", aligned. Multi-file output is
// separated by a "== path ==" header; failed files print their error.
std::string format_table(const std::vector<FileReport>& files, const std::vector<std::string>& extra_headers = {});

// `umm get` human form: values only (one summary per requested property).
std::string format_values(const std::vector<FileReport>& files);

// One JSON document per invocation:
//   {"schema_version":1,"command":"read","files":[{"path":..,"ok":true,
//     "properties":[{"id":..,"value":..}]}]}
// `extra` lets read-type commands without file reports (version, doctor)
// attach their payload fields at the top level.
Json make_document(const std::string& command, const std::vector<FileReport>& files,
                   Json::Object extra = {});

}  // namespace umm_cli
