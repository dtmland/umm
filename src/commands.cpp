#include "commands.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "batch.hpp"
#include "config.hpp"
#include "output.hpp"
#include "property.hpp"
#include "umm/umm.hpp"
#include "value_format.hpp"

#ifndef UMM_EXIFTOOL_TESTED_VERSION
#define UMM_EXIFTOOL_TESTED_VERSION "13.59"
#endif

#ifndef UMM_CLI_VERSION
#error "UMM_CLI_VERSION is set by CMake from project(umm VERSION)"
#endif

namespace umm_cli {

namespace fs = std::filesystem;

namespace {

umm::Result<void> check_file(const fs::path& p) {
  std::error_code ec;
  if (!fs::exists(p, ec))
    return umm::Error{umm::ErrorCode::io_not_found, "no such file: " + p.string(), "", ""};
  if (fs::is_directory(p, ec))
    return umm::Error{umm::ErrorCode::io_read_failed,
                      "is a directory (use --recursive): " + p.string(), "", ""};
  return {};
}

umm::ReadOptions read_options(const ParsedArgs& args) {
  umm::ReadOptions options;
  options.backend = args.backend;
  options.report_casts = args.options.count("report-casts") != 0;
  return options;
}

Json entries_json(const std::vector<umm::BaseEntry>& entries);
std::string format_entries(const std::vector<umm::BaseEntry>& entries);
Json cast_candidates_json(const std::vector<umm::CastCandidate>& cs);
std::string format_cast_candidates(const std::vector<umm::CastCandidate>& cs);

const char* cast_direction_name(umm::CastDirection d) noexcept {
  switch (d) {
    case umm::CastDirection::up:
      return "up";
    case umm::CastDirection::down:
      return "down";
    case umm::CastDirection::side:
      return "side";
  }
  return "up";
}

const char* cast_status_name(umm::CastStatus s) noexcept {
  switch (s) {
    case umm::CastStatus::can_cast:
      return "can_cast";
    case umm::CastStatus::needs_force:
      return "needs_force";
    case umm::CastStatus::equal:
      return "equal";
    case umm::CastStatus::source_empty:
      return "source_empty";
    case umm::CastStatus::target_not_storable:
      return "target_not_storable";
    case umm::CastStatus::ambiguous:
      return "ambiguous";
  }
  return "can_cast";
}

std::optional<umm::CastDirection> parse_cast_direction(std::string_view s) {
  if (s == "up") return umm::CastDirection::up;
  if (s == "down") return umm::CastDirection::down;
  if (s == "side") return umm::CastDirection::side;
  return std::nullopt;
}

std::vector<std::string> split_groups(const std::string& text) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&] {
    while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());
    while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
    if (!cur.empty()) out.push_back(cur);
    cur.clear();
  };
  for (char c : text) {
    if (c == ',')
      flush();
    else
      cur += c;
  }
  flush();
  return out;
}

const char* datatype_name(umm::Datatype d) noexcept {
  switch (d) {
    case umm::Datatype::text:
      return "text";
    case umm::Datatype::lang_alt:
      return "lang_alt";
    case umm::Datatype::text_list:
      return "text_list";
    case umm::Datatype::integer:
      return "integer";
    case umm::Datatype::real:
      return "real";
    case umm::Datatype::boolean:
      return "boolean";
    case umm::Datatype::rational:
      return "rational";
    case umm::Datatype::date_time:
      return "date_time";
    case umm::Datatype::structure:
      return "structure";
    case umm::Datatype::structure_list:
      return "structure_list";
  }
  return "text";
}

const char* cardinality_name(umm::Cardinality c) noexcept {
  switch (c) {
    case umm::Cardinality::one:
      return "one";
    case umm::Cardinality::many:
      return "many";
  }
  return "one";
}

struct LayerFilter {
  bool representations{true};
  bool casts{true};
  bool cross_media{true};
};

std::optional<LayerFilter> parse_layers(const std::string& text, std::string* error) {
  LayerFilter out{false, false, false};
  const std::vector<std::string> names = split_groups(text);
  if (names.empty()) {
    if (error) *error = "--layers needs a comma-separated list of representations, casts, cross-media";
    return std::nullopt;
  }
  for (const std::string& name : names) {
    if (name == "representations")
      out.representations = true;
    else if (name == "casts")
      out.casts = true;
    else if (name == "cross-media")
      out.cross_media = true;
    else {
      if (error)
        *error = "unknown layer '" + name + "' (expected representations, casts, cross-media)";
      return std::nullopt;
    }
  }
  return out;
}

Json property_value_json(const umm::PropertyValue& property) {
  Json::Array srcs;
  for (const umm::SourceRef& s : property.sources) {
    srcs.emplace_back(Json(Json::Object{{"base_key", Json(s.base_key)},
                                        {"backend", Json(s.backend)},
                                        {"container", Json(s.container)}}));
  }
  Json::Object o{{"value", value_to_json(property.value)},
                 {"resolution", Json(resolution_name(property.resolution))},
                 {"sources", Json(std::move(srcs))}};
  if (!property.preferred_source.empty())
    o.emplace_back("preferred_source", Json(property.preferred_source));
  return Json(std::move(o));
}

Json definition_json(const umm::PropertyDef& def) {
  Json::Object reps{{"xmp_namespace", Json(std::string(def.representations.xmp_namespace))},
                    {"xmp_property", Json(std::string(def.representations.xmp_property))},
                    {"iim_dataset", Json(std::string(def.representations.iim_dataset))},
                    {"exif_tag", Json(std::string(def.representations.exif_tag))},
                    {"quicktime_key", Json(std::string(def.representations.quicktime_key))},
                    {"ebucore", Json(std::string(def.representations.ebucore))}};
  return Json(Json::Object{{"id", Json(std::string(def.id))},
                           {"standard", Json(std::string(def.standard))},
                           {"standard_version", Json(std::string(def.standard_version))},
                           {"schema", Json(std::string(def.schema))},
                           {"standard_property_name", Json(std::string(def.standard_property_name))},
                           {"datatype", Json(datatype_name(def.datatype))},
                           {"cardinality", Json(cardinality_name(def.cardinality))},
                           {"representations", Json(std::move(reps))}});
}

Json struct_fields_json(const std::vector<umm::StructFieldMap>& fields) {
  Json::Array a;
  for (const auto& f : fields) {
    a.emplace_back(Json(Json::Object{{"id", Json(f.id)},
                                     {"name", Json(f.name)},
                                     {"struct_name", Json(f.struct_name)},
                                     {"xmp_property", Json(f.xmp_property)},
                                     {"et_tag", Json(f.et_tag)},
                                     {"exif_tag", Json(f.exif_tag)}}));
  }
  return Json(std::move(a));
}

Json representations_json(const std::vector<umm::RepresentationMap>& reps) {
  Json::Array a;
  for (const auto& r : reps) {
    Json::Object o{{"family", Json(r.family)},
                   {"key", Json(r.key)},
                   {"path", Json(r.path)},
                   {"citation", Json(r.citation)},
                   {"read_rank", Json(r.read_rank)},
                   {"write_target", Json(r.write_target)}};
    if (r.value) o.emplace_back("value", Json(*r.value));
    a.emplace_back(Json(std::move(o)));
  }
  return Json(std::move(a));
}

Json casts_json(const std::vector<umm::CastLinkMap>& casts) {
  Json::Array a;
  for (const auto& c : casts) {
    Json::Object o{{"group", Json(c.group)},
                   {"direction", Json(cast_direction_name(c.direction))},
                   {"partner", Json(c.partner)},
                   {"heuristic", Json(c.heuristic)},
                   {"citation", Json(c.citation)}};
    if (c.status) o.emplace_back("status", Json(cast_status_name(*c.status)));
    if (!c.source_preview.empty()) o.emplace_back("source_preview", Json(c.source_preview));
    if (!c.target_preview.empty()) o.emplace_back("target_preview", Json(c.target_preview));
    a.emplace_back(Json(std::move(o)));
  }
  return Json(std::move(a));
}

Json layers_json(const umm::PropertyLayers& layers, const LayerFilter& filter) {
  Json::Object o{{"id", Json(layers.id)}, {"definition", definition_json(layers.definition)}};
  if (filter.representations) {
    o.emplace_back("struct_fields", struct_fields_json(layers.struct_fields));
    o.emplace_back("representations", representations_json(layers.representations));
  }
  if (filter.casts) {
    o.emplace_back("casts", casts_json(layers.casts));
    if (!layers.cast_groups.empty())
      o.emplace_back("cast_groups", cast_candidates_json(layers.cast_groups));
  }
  if (layers.value) o.emplace_back("value", property_value_json(*layers.value));
  if (!layers.consumed.empty()) o.emplace_back("consumed", entries_json(layers.consumed));
  return Json(std::move(o));
}

Json description_json(const umm::PropertyDescription& d, const LayerFilter& filter) {
  Json::Object o{{"layers", layers_json(d.layers, filter)}};
  if (filter.cross_media && d.cross_media) {
    Json::Object cm{{"accessor", Json(d.cross_media->accessor)},
                    {"tier", Json(d.cross_media->tier)}};
    if (!d.cross_media->other.id.empty())
      cm.emplace_back("other", layers_json(d.cross_media->other, filter));
    o.emplace_back("cross_media", Json(std::move(cm)));
  }
  return Json(std::move(o));
}

Json property_map_json(const umm::PropertyMap& map, const LayerFilter& filter) {
  Json::Array props;
  for (const auto& p : map.properties) props.emplace_back(description_json(p, filter));
  return Json(Json::Object{{"query", Json(map.query)}, {"properties", Json(std::move(props))}});
}

void format_kv(std::ostringstream& os, const std::string& indent, const std::string& key,
               const std::string& value) {
  os << indent << key;
  for (std::size_t i = key.size(); i < 24; ++i) os << ' ';
  os << "  " << value << "\n";
}

void format_definition(std::ostringstream& os, const umm::PropertyDef& def, const std::string& indent) {
  os << indent << "DEFINITION\n";
  format_kv(os, indent + "  ", "id", std::string(def.id));
  format_kv(os, indent + "  ", "standard", std::string(def.standard));
  format_kv(os, indent + "  ", "standard_version", std::string(def.standard_version));
  format_kv(os, indent + "  ", "schema", std::string(def.schema));
  format_kv(os, indent + "  ", "standard_property_name", std::string(def.standard_property_name));
  format_kv(os, indent + "  ", "datatype", datatype_name(def.datatype));
  format_kv(os, indent + "  ", "cardinality", cardinality_name(def.cardinality));
}

void format_layers_human(std::ostringstream& os, const umm::PropertyLayers& layers,
                         const LayerFilter& filter, const std::string& indent) {
  os << indent << layers.id << "\n";
  format_definition(os, layers.definition, indent);
  if (layers.value) {
    os << indent << "VALUE\n";
    format_kv(os, indent + "  ", "summary", value_summary(layers.value->value));
    format_kv(os, indent + "  ", "resolution", resolution_name(layers.value->resolution));
  }
  if (!layers.consumed.empty()) {
    os << indent << "CONSUMED\n";
    os << indent << format_entries(layers.consumed);
  }
  if (filter.representations) {
    os << indent << "REPRESENTATIONS\n";
    if (layers.representations.empty()) {
      os << indent << "  (none)\n";
    } else {
      for (const auto& r : layers.representations) {
        std::string line = indent + "  " + r.family + "  " + r.key;
        if (!r.path.empty()) line += "  " + r.path;
        line += "  rank " + std::to_string(r.read_rank);
        line += r.write_target ? "  write" : "  no-write";
        if (r.value) line += "  " + *r.value;
        os << line << "\n";
      }
    }
    if (!layers.struct_fields.empty()) {
      os << indent << "STRUCT_FIELDS\n";
      for (const auto& f : layers.struct_fields) {
        os << indent << "  " << f.id << "  " << f.name << "  " << f.struct_name;
        if (!f.xmp_property.empty()) os << "  " << f.xmp_property;
        if (!f.et_tag.empty()) os << "  " << f.et_tag;
        if (!f.exif_tag.empty()) os << "  " << f.exif_tag;
        os << "\n";
      }
    }
  }
  if (filter.casts) {
    os << indent << "CASTS\n";
    if (layers.casts.empty()) {
      os << indent << "  (none)\n";
    } else {
      for (const auto& c : layers.casts) {
        os << indent << "  " << c.group << "  " << cast_direction_name(c.direction) << "  "
           << c.partner;
        if (!c.heuristic.empty()) os << "  " << c.heuristic;
        if (c.status) os << "  " << cast_status_name(*c.status);
        os << "\n";
      }
    }
    if (!layers.cast_groups.empty()) {
      os << indent << "CAST_GROUPS\n";
      os << format_cast_candidates(layers.cast_groups);
    }
  }
}

void format_description_human(std::ostringstream& os, const umm::PropertyDescription& d,
                              const LayerFilter& filter) {
  format_layers_human(os, d.layers, filter, "");
  if (filter.cross_media && d.cross_media) {
    os << "CROSS-MEDIA\n";
    format_kv(os, "  ", "accessor", d.cross_media->accessor);
    format_kv(os, "  ", "tier", std::to_string(d.cross_media->tier));
    if (!d.cross_media->other.id.empty()) {
      format_kv(os, "  ", "other", d.cross_media->other.id);
      format_layers_human(os, d.cross_media->other, filter, "  ");
    }
  }
}

std::string format_property_map(const umm::PropertyMap& map, const LayerFilter& filter) {
  std::ostringstream os;
  os << "QUERY  " << map.query << "\n";
  for (const auto& p : map.properties) {
    os << "\n";
    format_description_human(os, p, filter);
  }
  return os.str();
}

umm::CastOptions cast_options(const ParsedArgs& args) {
  umm::CastOptions options;
  options.dry_run = args.options.count("apply") == 0;
  options.force = args.options.count("force") != 0;
  options.include_approximate = args.options.count("include-approximate") != 0;
  auto it = args.options.find("group");
  if (it != args.options.end()) options.groups = split_groups(it->second);
  return options;
}

Json cast_candidate_json(const umm::CastCandidate& c) {
  Json::Object o{{"group", Json(c.group)},
                 {"direction", Json(cast_direction_name(c.direction))},
                 {"status", Json(cast_status_name(c.status))}};
  if (!c.source_id.empty()) o.emplace_back("source_id", Json(c.source_id));
  if (!c.target_id.empty()) o.emplace_back("target_id", Json(c.target_id));
  if (!c.source_preview.empty()) o.emplace_back("source_preview", Json(c.source_preview));
  if (!c.target_preview.empty()) o.emplace_back("target_preview", Json(c.target_preview));
  if (!c.notes.empty()) {
    Json::Array notes;
    for (const std::string& n : c.notes) notes.emplace_back(Json(n));
    o.emplace_back("notes", Json(std::move(notes)));
  }
  return Json(std::move(o));
}

Json cast_candidates_json(const std::vector<umm::CastCandidate>& cs) {
  Json::Array a;
  for (const auto& c : cs) a.emplace_back(cast_candidate_json(c));
  return Json(std::move(a));
}

std::string format_cast_candidates(const std::vector<umm::CastCandidate>& cs) {
  if (cs.empty()) return "no cast candidates\n";
  std::size_t gw = 5, dw = 9, sw = 6, srcw = 6;
  for (const auto& c : cs) {
    if (c.group.size() > gw) gw = c.group.size();
    dw = std::max(dw, std::string_view(cast_direction_name(c.direction)).size());
    sw = std::max(sw, std::string_view(cast_status_name(c.status)).size());
    if (c.source_id.size() > srcw) srcw = c.source_id.size();
  }
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  std::string out = pad("GROUP", gw) + "  " + pad("DIRECTION", dw) + "  " + pad("STATUS", sw) +
                    "  " + pad("SOURCE", srcw) + "  TARGET\n";
  for (const auto& c : cs) {
    std::string line = pad(c.group, gw) + "  " + pad(cast_direction_name(c.direction), dw) + "  " +
                       pad(cast_status_name(c.status), sw) + "  " + pad(c.source_id, srcw) + "  " +
                       c.target_id;
    while (!line.empty() && line.back() == ' ') line.pop_back();
    out += line + "\n";
    if (!c.source_preview.empty()) out += "  source_preview  " + c.source_preview + "\n";
    if (!c.target_preview.empty()) out += "  target_preview  " + c.target_preview + "\n";
    for (const std::string& n : c.notes) out += "  note  " + n + "\n";
  }
  return out;
}

void emit_reports(const ParsedArgs& args, const std::vector<FileReport>& reports,
                  const std::vector<ExitCode>& failures, std::size_t total, std::ostream& out,
                  std::ostream& err, const std::vector<std::string>& extra_headers, bool values_only) {
  if (args.json) {
    out << make_document(std::string(args.command->name), reports).dump() << "\n";
    return;
  }
  std::vector<FileReport> printable;
  for (const FileReport& f : reports) {
    if (!f.ok) err << "umm " << args.command->name << ": " << f.error << "\n";
    if (f.ok || (values_only && !f.properties.empty())) {
      FileReport row = f;
      row.ok = true;
      printable.push_back(std::move(row));
    }
  }
  if (!printable.empty())
    out << (values_only ? format_values(printable) : format_table(printable, extra_headers));
  if (!failures.empty() && total > 1)
    err << "umm " << args.command->name << ": " << failures.size() << " of " << total
        << " file(s) failed\n";
}

ExitCode run_version(const ParsedArgs& args, std::ostream& out) {
  const std::string cli = UMM_CLI_VERSION;
  const std::string lib{umm::version()};
  std::vector<umm::Registry::StandardInfo> rows = umm::registry().standards();
  if (args.json) {
    Json::Array standards;
    for (const auto& s : rows) {
      standards.emplace_back(Json(Json::Object{{"standard", Json(std::string(s.standard))},
                                               {"version", Json(std::string(s.version))},
                                               {"source_document", Json(std::string(s.source_document))}}));
    }
    out << make_document("version", {},
                         {{"version", Json(cli)},
                          {"libumm", Json(lib)},
                          {"standards", Json(std::move(standards))}})
               .dump()
        << "\n";
    return ExitCode::ok;
  }
  out << "umm " << cli << "\nlibumm " << lib << "\n";
  if (rows.empty()) return ExitCode::ok;
  std::size_t sw = 8, vw = 7;
  for (const auto& s : rows) {
    if (s.standard.size() > sw) sw = s.standard.size();
    if (s.version.size() > vw) vw = s.version.size();
  }
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  out << "\n" << pad("STANDARD", sw) << "  " << pad("VERSION", vw) << "  SOURCE\n";
  for (const auto& s : rows) {
    std::string line = pad(std::string(s.standard), sw) + "  " + pad(std::string(s.version), vw) +
                       "  " + std::string(s.source_document);
    while (!line.empty() && line.back() == ' ') line.pop_back();
    out << line << "\n";
  }
  return ExitCode::ok;
}

ExitCode run_read(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  const bool sources = args.options.count("sources") != 0;
  const bool report_casts = args.options.count("report-casts") != 0;
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  umm::ReadOptions options = read_options(args);
  std::string human_casts;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, options);
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    for (const std::string& id : r.value().propertyIds()) {
      std::optional<umm::PropertyValue> pv = r.value().get(id);
      if (pv) report.properties.push_back(property_row(id, *pv, sources));
    }
    if (report_casts) {
      report.json_extra.emplace_back("cast_candidates",
                                     cast_candidates_json(r.value().castCandidates()));
      if (multi) human_casts += "== " + file.string() + " ==\n";
      human_casts += "CAST CANDIDATES  (preview, not stored)\n";
      human_casts += format_cast_candidates(r.value().castCandidates());
    }
    reports.push_back(std::move(report));
  }
  emit_reports(args, reports, failures, files.size(), out, err,
               sources ? std::vector<std::string>{"SOURCE", "RESOLUTION"} : std::vector<std::string>{},
               false);
  if (!args.json && report_casts && !human_casts.empty()) out << human_casts;
  return summarize(failures);
}

std::optional<umm::PropertyValue> fetch_property(const umm::Metadata& meta,
                                                 const ResolvedProperty& resolved) {
  if (resolved.is_accessor()) return (meta.*(resolved.getter))();
  return meta.get(resolved.property_id);
}

ExitCode run_get(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  std::vector<std::string> names(args.operands.begin() + 1, args.operands.end());
  std::vector<ResolvedProperty> resolved;
  for (const std::string& name : names) {
    umm::Result<ResolvedProperty> r = resolve_property(name, umm::MediaDomain::unknown);
    if (!r.ok()) {
      err << "umm get: " << r.error().message << "\n";
      if (args.json) {
        FileReport bad{"", false, r.error().message, {}};
        out << make_document("get", {bad}).dump() << "\n";
      }
      return exit_code_for(r.error().code);
    }
    resolved.push_back(r.value());
  }

  std::vector<fs::path> files = expand_operands({args.operands.front()}, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  umm::ReadOptions options = read_options(args);
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, options);
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    std::vector<std::string> missing;
    for (std::size_t i = 0; i < resolved.size(); ++i) {
      std::optional<umm::PropertyValue> pv = fetch_property(r.value(), resolved[i]);
      if (!pv) {
        missing.push_back(names[i]);
        continue;
      }
      report.properties.push_back(property_row(names[i], *pv, false));
    }
    if (!missing.empty()) {
      std::ostringstream msg;
      msg << "property not found:";
      for (const std::string& m : missing) msg << " " << m;
      report.ok = false;
      report.error = msg.str();
      failures.push_back(ExitCode::not_found);
    }
    reports.push_back(std::move(report));
  }
  emit_reports(args, reports, failures, files.size(), out, err, {}, true);
  return summarize(failures);
}

const char* access_name(umm::Access a) noexcept {
  switch (a) {
    case umm::Access::none:
      return "none";
    case umm::Access::read:
      return "read";
    case umm::Access::read_write:
      return "read_write";
    case umm::Access::create:
      return "create";
  }
  return "none";
}

Json categories_json(const umm::CategoryAccess& c) {
  return Json(Json::Object{{"exif", Json(access_name(c.exif))},
                           {"iptc_iim", Json(access_name(c.iptc_iim))},
                           {"xmp", Json(access_name(c.xmp))},
                           {"icc", Json(access_name(c.icc))},
                           {"thumbnail", Json(access_name(c.thumbnail))}});
}

Json location_json(const umm::LocationAccess& loc) {
  return Json(Json::Object{{"gps_exif", Json(access_name(loc.gps_exif))},
                           {"named_place", Json(access_name(loc.named_place))},
                           {"xmp_location", Json(access_name(loc.xmp_location))},
                           {"container_gps", Json(access_name(loc.container_gps))},
                           {"geotiff", Json(access_name(loc.geotiff))}});
}

Json entries_json(const std::vector<umm::BaseEntry>& entries) {
  Json::Array a;
  for (const auto& e : entries) {
    a.emplace_back(Json(Json::Object{{"family", Json(e.key.family)},
                                     {"key", Json(e.key.key)},
                                     {"value", Json(e.value)},
                                     {"cast_source", Json(e.cast_source)}}));
  }
  return Json(std::move(a));
}

std::string format_entries(const std::vector<umm::BaseEntry>& entries) {
  std::size_t fw = 6, kw = 3;
  for (const auto& e : entries) {
    if (e.key.family.size() > fw) fw = e.key.family.size();
    if (e.key.key.size() > kw) kw = e.key.key.size();
  }
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  std::string out = pad("FAMILY", fw) + "  " + pad("KEY", kw) + "  VALUE\n";
  for (const auto& e : entries) {
    std::string line = pad(e.key.family, fw) + "  " + pad(e.key.key, kw) + "  " + e.value;
    while (!line.empty() && line.back() == ' ') line.pop_back();
    out += line + "\n";
  }
  return out;
}

Json conflicts_json(const std::vector<umm::ConflictEntry>& entries) {
  Json::Array a;
  for (const auto& e : entries) {
    Json::Array cands;
    for (const auto& c : e.candidates) {
      Json::Array srcs;
      for (const auto& s : c.sources) {
        srcs.emplace_back(Json(Json::Object{{"base_key", Json(s.base_key)},
                                            {"backend", Json(s.backend)},
                                            {"container", Json(s.container)}}));
      }
      cands.emplace_back(Json(Json::Object{{"value", value_to_json(c.value)},
                                           {"family", Json(c.family)},
                                           {"primary_key", Json(c.primary_key)},
                                           {"sources", Json(std::move(srcs))}}));
    }
    a.emplace_back(Json(Json::Object{{"property_id", Json(e.property_id)},
                                     {"resolution", Json(resolution_name(e.resolution))},
                                     {"preferred_source", Json(e.preferred_source)},
                                     {"candidates", Json(std::move(cands))}}));
  }
  return Json(std::move(a));
}

std::string format_conflicts(const std::vector<umm::ConflictEntry>& entries) {
  if (entries.empty()) return "no conflicts\n";
  std::size_t iw = 8;
  for (const auto& e : entries) {
    if (e.property_id.size() > iw) iw = e.property_id.size();
  }
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  std::string out = pad("PROPERTY", iw) + "  RESOLUTION  PREFERRED\n";
  for (const auto& e : entries) {
    out += pad(e.property_id, iw) + "  " + pad(resolution_name(e.resolution), 11) + "  " +
           e.preferred_source + "\n";
    for (const auto& c : e.candidates) {
      out += "  " + c.family + "  " + c.primary_key + "  " + value_summary(c.value) + "\n";
    }
  }
  return out;
}

Json capabilities_json(const umm::Capabilities& caps) {
  Json::Array backends;
  for (const auto& b : caps.backends) {
    backends.emplace_back(Json(Json::Object{{"backend", Json(b.backend)},
                                            {"available", Json(b.available)},
                                            {"identify_only", Json(b.identify_only)},
                                            {"categories", categories_json(b.categories)},
                                            {"location", location_json(b.location)},
                                            {"notes", Json(b.notes)}}));
  }
  return Json(Json::Object{{"file_type", Json(caps.file_type)},
                           {"preferred_backend", Json(caps.preferred_backend)},
                           {"sidecar_recommended", Json(caps.sidecar_recommended)},
                           {"backends", Json(std::move(backends))}});
}

std::string format_capabilities(const umm::Capabilities& caps) {
  std::ostringstream os;
  os << "TYPE  " << caps.file_type << "\nPREFERRED  " << caps.preferred_backend
     << "\nSIDECAR_RECOMMENDED  " << (caps.sidecar_recommended ? "yes" : "no") << "\n\n";
  os << "BACKEND    AVAILABLE  EXIF        IPTC-IIM    XMP         GPS_EXIF    NAMED_PLACE  NOTES\n";
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  for (const auto& b : caps.backends) {
    std::string line = pad(b.backend, 10) + "  " + pad(b.available ? "yes" : "no", 9) + "  " +
                       pad(access_name(b.categories.exif), 11) + "  " +
                       pad(access_name(b.categories.iptc_iim), 11) + "  " +
                       pad(access_name(b.categories.xmp), 11) + "  " +
                       pad(access_name(b.location.gps_exif), 11) + "  " +
                       pad(access_name(b.location.named_place), 11) + "  " + b.notes;
    while (!line.empty() && line.back() == ' ') line.pop_back();
    os << line << "\n";
  }
  return os.str();
}

void emit_inspect(const ParsedArgs& args, const std::vector<FileReport>& reports,
                  const std::vector<ExitCode>& failures, std::size_t total, const std::string& human,
                  std::ostream& out, std::ostream& err) {
  if (args.json) {
    out << make_document(std::string(args.command->name), reports).dump() << "\n";
    return;
  }
  for (const FileReport& f : reports) {
    if (!f.ok) err << "umm " << args.command->name << ": " << f.error << "\n";
  }
  if (!human.empty()) out << human;
  if (!failures.empty() && total > 1)
    err << "umm " << args.command->name << ": " << failures.size() << " of " << total
        << " file(s) failed\n";
}

ExitCode run_dump(const ParsedArgs& args, bool unmapped_only, std::ostream& out, std::ostream& err) {
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  umm::ReadOptions options = read_options(args);
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, options);
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    const std::vector<umm::BaseEntry>& entries =
        unmapped_only ? r.value().dumpUnmapped() : r.value().dumpAll();
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("entries", entries_json(entries));
    reports.push_back(report);
    if (multi) human += "== " + file.string() + " ==\n";
    human += format_entries(entries);
  }
  emit_inspect(args, reports, failures, files.size(), human, out, err);
  return summarize(failures);
}

ExitCode run_conflicts(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  const bool fail_on = args.options.count("fail-on-conflict") != 0;
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  umm::ReadOptions options = read_options(args);
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::ConflictReport> r = umm::detectConflict(file, options);
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("conflicts", conflicts_json(r.value().entries));
    reports.push_back(report);
    if (multi) human += "== " + file.string() + " ==\n";
    human += format_conflicts(r.value().entries);
    if (fail_on && !r.value().entries.empty()) failures.push_back(ExitCode::semantics);
  }
  emit_inspect(args, reports, failures, files.size(), human, out, err);
  return summarize(failures);
}

umm::WriteOptions write_options(const ParsedArgs& args) {
  umm::WriteOptions options;
  options.backend = args.backend;
  options.dry_run = args.options.count("dry-run") != 0;
  return options;
}

std::optional<umm::StoragePolicy> parse_policy(const ParsedArgs& args, std::ostream& err) {
  auto it = args.options.find("policy");
  if (it == args.options.end()) return umm::StoragePolicy::preferred;
  const std::string& v = it->second;
  if (v == "embedded") return umm::StoragePolicy::embedded_only;
  if (v == "sidecar") return umm::StoragePolicy::sidecar_only;
  if (v == "sidecar-required") return umm::StoragePolicy::sidecar_required;
  if (v == "preferred") return umm::StoragePolicy::preferred;
  err << "umm: --policy must be embedded|sidecar|sidecar-required|preferred, got '" << v << "'\n";
  return std::nullopt;
}

std::optional<umm::SyncDirection> parse_direction(const ParsedArgs& args, std::ostream& err) {
  auto it = args.options.find("direction");
  if (it == args.options.end()) return umm::SyncDirection::both;
  const std::string& v = it->second;
  if (v == "both") return umm::SyncDirection::both;
  if (v == "embedded-to-sidecar") return umm::SyncDirection::embedded_to_sidecar;
  if (v == "sidecar-to-embedded") return umm::SyncDirection::sidecar_to_embedded;
  err << "umm: --direction must be both|embedded-to-sidecar|sidecar-to-embedded, got '" << v
      << "'\n";
  return std::nullopt;
}

const char* storage_method_name(umm::StorageDecision::Method m) noexcept {
  switch (m) {
    case umm::StorageDecision::Method::embedded:
      return "embedded";
    case umm::StorageDecision::Method::sidecar:
      return "sidecar";
    case umm::StorageDecision::Method::mixed:
      return "mixed";
  }
  return "embedded";
}

Json written_json(const std::vector<umm::BaseKey>& keys) {
  Json::Array a;
  for (const auto& k : keys)
    a.emplace_back(Json(Json::Object{{"family", Json(k.family)}, {"key", Json(k.key)}}));
  return Json(std::move(a));
}

Json write_report_json(const umm::WriteReport& report) {
  Json::Array formats;
  for (const auto& f : report.decision.formats) formats.emplace_back(Json(f));
  return Json(Json::Object{{"method", Json(storage_method_name(report.decision.method))},
                           {"backend", Json(report.decision.backend)},
                           {"formats", Json(std::move(formats))},
                           {"written", written_json(report.written)}});
}

std::string format_write_report(const umm::WriteReport& report) {
  std::ostringstream os;
  os << "METHOD  " << storage_method_name(report.decision.method) << "\nBACKEND  "
     << report.decision.backend << "\nFORMATS  ";
  for (std::size_t i = 0; i < report.decision.formats.size(); ++i) {
    if (i) os << ", ";
    os << report.decision.formats[i];
  }
  os << "\nWRITTEN  ";
  for (std::size_t i = 0; i < report.written.size(); ++i) {
    if (i) os << ", ";
    os << report.written[i].key;
  }
  os << "\n";
  return os.str();
}

Json sync_report_json(const umm::SyncReport& report) {
  Json::Array formats;
  for (const auto& f : report.decision.formats) formats.emplace_back(Json(f));
  Json::Array carriers;
  for (const auto& c : report.carriers) {
    carriers.emplace_back(
        Json(Json::Object{{"container", Json(c.container)}, {"written", written_json(c.written)}}));
  }
  return Json(Json::Object{{"method", Json(storage_method_name(report.decision.method))},
                           {"backend", Json(report.decision.backend)},
                           {"formats", Json(std::move(formats))},
                           {"carriers", Json(std::move(carriers))}});
}

std::string format_sync_report(const umm::SyncReport& report) {
  std::ostringstream os;
  os << "METHOD  " << storage_method_name(report.decision.method) << "\nBACKEND  "
     << report.decision.backend << "\n";
  for (const auto& c : report.carriers) {
    os << "CARRIER  " << c.container;
    for (std::size_t i = 0; i < c.written.size(); ++i) os << (i ? ", " : "  ") << c.written[i].key;
    os << "\n";
  }
  return os.str();
}

bool is_assign_token(const std::string& tok) {
  auto eq = tok.find('=');
  if (eq != std::string::npos && eq > 0) return true;
  return resolve_property(tok, umm::MediaDomain::unknown).ok();
}

struct Assignment {
  ResolvedProperty property;
  umm::Value value;
};

umm::Result<std::vector<Assignment>> parse_assignments(const std::vector<std::string>& tokens) {
  std::vector<Assignment> out;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const std::string& tok = tokens[i];
    auto eq = tok.find('=');
    if (eq != std::string::npos && eq > 0) {
      umm::Result<ResolvedProperty> p = resolve_property(tok.substr(0, eq), umm::MediaDomain::unknown);
      if (!p.ok()) return p.error();
      umm::Result<umm::Value> v = parse_value(p.value().datatype, tok.substr(eq + 1), false);
      if (!v.ok()) return v.error();
      out.push_back({p.value(), v.value()});
      continue;
    }
    umm::Result<ResolvedProperty> p = resolve_property(tok, umm::MediaDomain::unknown);
    if (!p.ok()) return p.error();
    if (i + 1 >= tokens.size())
      return umm::Error{umm::ErrorCode::invalid_value,
                        "missing JSON value for '" + tok + "' (use --json)", "", ""};
    ++i;
    umm::Result<umm::Value> v = parse_value(p.value().datatype, tokens[i], true);
    if (!v.ok()) return v.error();
    out.push_back({p.value(), v.value()});
  }
  return out;
}

std::size_t first_assign_index(const std::vector<std::string>& operands) {
  for (std::size_t i = 0; i < operands.size(); ++i)
    if (is_assign_token(operands[i])) return i;
  return operands.size();
}

void emit_mutate(const ParsedArgs& args, const std::vector<FileReport>& reports,
                 const std::vector<ExitCode>& failures, std::size_t total, const std::string& human,
                 bool emit_report, std::ostream& out, std::ostream& err) {
  if (args.json && emit_report) {
    out << make_document(std::string(args.command->name), reports).dump() << "\n";
  } else if (emit_report && !human.empty()) {
    out << human;
  }
  if (!args.json || !emit_report) {
    for (const FileReport& f : reports)
      if (!f.ok) err << "umm " << args.command->name << ": " << f.error << "\n";
  }
  if (!failures.empty() && total > 1)
    err << "umm " << args.command->name << ": " << failures.size() << " of " << total
        << " file(s) failed\n";
}

ExitCode run_set(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  auto policy = parse_policy(args, err);
  if (!policy) return ExitCode::usage;
  const std::size_t split = first_assign_index(args.operands);
  if (split == 0 || split == args.operands.size()) {
    err << "umm set: need FILE… and ASSIGN…\n";
    return ExitCode::usage;
  }
  std::vector<std::string> file_ops(args.operands.begin(), args.operands.begin() + static_cast<std::ptrdiff_t>(split));
  std::vector<std::string> assign_toks(args.operands.begin() + static_cast<std::ptrdiff_t>(split),
                                       args.operands.end());
  umm::Result<std::vector<Assignment>> assigns = parse_assignments(assign_toks);
  if (!assigns.ok()) {
    err << "umm set: " << assigns.error().message << "\n";
    return exit_code_for(assigns.error().code);
  }
  umm::WriteOptions options = write_options(args);
  options.policy = *policy;
  const bool dry = options.dry_run;
  std::vector<fs::path> files = expand_operands(file_ops, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, read_options(args));
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    umm::Metadata meta = std::move(r).value();
    bool ok = true;
    for (const Assignment& a : assigns.value()) {
      umm::Result<void> s = apply_set(meta, a.property, a.value);
      if (!s.ok()) {
        reports.push_back({file.string(), false, s.error().message, {}});
        failures.push_back(exit_code_for(s.error().code));
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    umm::Result<umm::WriteReport> wr = umm::write(file, meta, options);
    if (!wr.ok()) {
      reports.push_back({file.string(), false, wr.error().message, {}});
      failures.push_back(exit_code_for(wr.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("report", write_report_json(wr.value()));
    reports.push_back(report);
    if (dry) {
      if (multi) human += "== " + file.string() + " ==\n";
      human += format_write_report(wr.value());
    }
  }
  emit_mutate(args, reports, failures, files.size(), human, dry, out, err);
  return summarize(failures);
}

ExitCode run_rm(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  auto policy = parse_policy(args, err);
  if (!policy) return ExitCode::usage;
  const std::size_t split = first_assign_index(args.operands);
  if (split == 0 || split == args.operands.size()) {
    err << "umm rm: need FILE… and PROPERTY…\n";
    return ExitCode::usage;
  }
  std::vector<std::string> file_ops(args.operands.begin(), args.operands.begin() + static_cast<std::ptrdiff_t>(split));
  std::vector<ResolvedProperty> props;
  for (std::size_t i = split; i < args.operands.size(); ++i) {
    umm::Result<ResolvedProperty> p = resolve_property(args.operands[i], umm::MediaDomain::unknown);
    if (!p.ok()) {
      err << "umm rm: " << p.error().message << "\n";
      return exit_code_for(p.error().code);
    }
    props.push_back(p.value());
  }
  umm::WriteOptions options = write_options(args);
  options.policy = *policy;
  const bool dry = options.dry_run;
  std::vector<fs::path> files = expand_operands(file_ops, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, read_options(args));
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    umm::Metadata meta = std::move(r).value();
    bool ok = true;
    for (const ResolvedProperty& p : props) {
      umm::Result<void> s = apply_remove(meta, p);
      if (!s.ok()) {
        reports.push_back({file.string(), false, s.error().message, {}});
        failures.push_back(exit_code_for(s.error().code));
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    umm::Result<umm::WriteReport> wr = umm::write(file, meta, options);
    if (!wr.ok()) {
      reports.push_back({file.string(), false, wr.error().message, {}});
      failures.push_back(exit_code_for(wr.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("report", write_report_json(wr.value()));
    reports.push_back(report);
    if (dry) {
      if (multi) human += "== " + file.string() + " ==\n";
      human += format_write_report(wr.value());
    }
  }
  emit_mutate(args, reports, failures, files.size(), human, dry, out, err);
  return summarize(failures);
}

ExitCode run_merge(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  auto policy = parse_policy(args, err);
  if (!policy) return ExitCode::usage;
  const bool has_use = args.options.count("use") != 0;
  const bool has_value = args.options.count("value") != 0;
  if (has_use == has_value) {
    err << "umm merge: exactly one of --use BASEKEY or --value V is required\n";
    return ExitCode::usage;
  }
  if (args.operands.size() < 2) {
    err << "umm merge: need FILE and PROPERTY\n";
    return ExitCode::usage;
  }
  if (args.operands.size() > 2) {
    err << "umm merge: extra operand '" << args.operands[2] << "'\n";
    return ExitCode::usage;
  }
  umm::Result<ResolvedProperty> prop = resolve_property(args.operands[1], umm::MediaDomain::unknown);
  if (!prop.ok()) {
    err << "umm merge: " << prop.error().message << "\n";
    return exit_code_for(prop.error().code);
  }
  if (prop.value().is_accessor()) {
    err << "umm merge: PROPERTY must be a full property id, not an accessor\n";
    return ExitCode::usage;
  }
  auto cit = args.options.find("container");
  if (cit != args.options.end() && cit->second != "embedded" && cit->second != "sidecar") {
    err << "umm merge: --container must be embedded or sidecar\n";
    return ExitCode::usage;
  }
  const std::string container = cit == args.options.end() ? "" : cit->second;
  umm::WriteOptions wopts = write_options(args);
  wopts.policy = *policy;
  const bool dry = wopts.dry_run;
  std::vector<fs::path> files = expand_operands({args.operands.front()}, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    std::optional<umm::Result<umm::Metadata>> merged;
    if (has_use) {
      umm::Result<umm::ConflictReport> cr = umm::detectConflict(file, read_options(args));
      if (!cr.ok()) {
        reports.push_back({file.string(), false, cr.error().message, {}});
        failures.push_back(exit_code_for(cr.error().code));
        continue;
      }
      const umm::ConflictEntry* entry = nullptr;
      for (const auto& e : cr.value().entries)
        if (e.property_id == prop.value().property_id) entry = &e;
      if (!entry) {
        reports.push_back({file.string(), false,
                           "no conflict for " + prop.value().property_id, {}});
        failures.push_back(ExitCode::semantics);
        continue;
      }
      merged = umm::merge(cr.value().metadata, *entry, args.options.at("use"), container);
    } else {
      umm::Result<umm::Metadata> r = umm::read(file, read_options(args));
      if (!r.ok()) {
        reports.push_back({file.string(), false, r.error().message, {}});
        failures.push_back(exit_code_for(r.error().code));
        continue;
      }
      umm::Result<umm::Value> v =
          parse_value(prop.value().datatype, args.options.at("value"), false);
      if (!v.ok()) {
        reports.push_back({file.string(), false, v.error().message, {}});
        failures.push_back(exit_code_for(v.error().code));
        continue;
      }
      merged = umm::merge(std::move(r).value(), prop.value().property_id, v.value());
    }
    if (!merged->ok()) {
      reports.push_back({file.string(), false, merged->error().message, {}});
      failures.push_back(exit_code_for(merged->error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    if (dry) {
      report.json_extra.emplace_back("merged", Json(true));
      reports.push_back(report);
      if (multi) human += "== " + file.string() + " ==\n";
      human += "merged " + prop.value().property_id + " (dry-run, not written)\n";
      continue;
    }
    umm::Result<umm::WriteReport> wr = umm::write(file, merged->value(), wopts);
    if (!wr.ok()) {
      reports.push_back({file.string(), false, wr.error().message, {}});
      failures.push_back(exit_code_for(wr.error().code));
      continue;
    }
    report.json_extra.emplace_back("report", write_report_json(wr.value()));
    reports.push_back(report);
  }
  emit_mutate(args, reports, failures, files.size(), human, dry || args.json, out, err);
  return summarize(failures);
}

ExitCode run_sync(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  auto direction = parse_direction(args, err);
  if (!direction) return ExitCode::usage;
  umm::SyncOptions options;
  options.backend = args.backend;
  options.direction = *direction;
  options.dry_run = args.options.count("dry-run") != 0;
  const bool dry = options.dry_run;
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::SyncReport> r = umm::synchronize(file, options);
    if (!r.ok()) {
      std::string msg = r.error().message;
      if (r.error().code == umm::ErrorCode::conflict_unresolved &&
          msg.find("merge") == std::string::npos)
        msg += " (use umm merge first)";
      reports.push_back({file.string(), false, msg, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("report", sync_report_json(r.value()));
    reports.push_back(report);
    if (dry || args.json) {
      if (multi) human += "== " + file.string() + " ==\n";
      human += format_sync_report(r.value());
    }
  }
  emit_mutate(args, reports, failures, files.size(), human, dry || args.json, out, err);
  return summarize(failures);
}

ExitCode run_caps(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = args.operands.size() > 1;
  for (const std::string& op : args.operands) {
    std::error_code ec;
    umm::Result<umm::Capabilities> r =
        fs::exists(op, ec) ? umm::capabilities(op) : umm::capabilitiesForType(op);
    if (!r.ok()) {
      reports.push_back({op, false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{op, true, "", {}};
    report.json_extra.emplace_back("capabilities", capabilities_json(r.value()));
    reports.push_back(report);
    if (multi) human += "== " + op + " ==\n";
    human += format_capabilities(r.value());
  }
  emit_inspect(args, reports, failures, args.operands.size(), human, out, err);
  return summarize(failures);
}

const char* match_kind_name(umm::TrackMatchKind k) noexcept {
  switch (k) {
    case umm::TrackMatchKind::exact:
      return "exact";
    case umm::TrackMatchKind::interpolated:
      return "interpolated";
    case umm::TrackMatchKind::nearest:
      return "nearest";
  }
  return "exact";
}

ExitCode run_geotag(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  auto policy = parse_policy(args, err);
  if (!policy) return ExitCode::usage;
  auto tit = args.options.find("track");
  if (tit == args.options.end() || tit->second.empty()) {
    err << "umm geotag: --track is required\n";
    return ExitCode::usage;
  }
  umm::MatchOptions match_opts;
  auto oit = args.options.find("offset");
  if (oit != args.options.end()) {
    const std::string& s = oit->second;
    if (s.empty()) {
      err << "umm geotag: --offset must be an integer number of minutes\n";
      return ExitCode::usage;
    }
    std::size_t idx = 0;
    try {
      long v = std::stol(s, &idx, 10);
      if (idx != s.size() || v < -24 * 60 || v > 24 * 60) throw std::out_of_range("offset");
      match_opts.naive_utc_offset_minutes = static_cast<int>(v);
    } catch (...) {
      err << "umm geotag: --offset must be an integer number of minutes\n";
      return ExitCode::usage;
    }
  }
  umm::Result<umm::Track> track = umm::importTrack(tit->second);
  if (!track.ok()) {
    err << "umm geotag: " << track.error().message << "\n";
    return exit_code_for(track.error().code);
  }
  umm::WriteOptions wopts = write_options(args);
  wopts.policy = *policy;
  const bool dry = wopts.dry_run;
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::Metadata> r = umm::read(file, read_options(args));
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    umm::Metadata meta = std::move(r).value();
    umm::Result<umm::TrackMatch> match = umm::matchTrack(meta, track.value(), match_opts);
    if (!match.ok()) {
      reports.push_back({file.string(), false, match.error().message, {}});
      failures.push_back(exit_code_for(match.error().code));
      continue;
    }
    std::vector<umm::Structure> items;
    if (const auto existing = meta.locationCreated()) {
      if (const auto* list = std::get_if<std::vector<umm::Structure>>(&existing->value.data))
        items = *list;
    }
    if (items.empty()) items.emplace_back();
    items.front().insert_or_assign("gpsLatitude", umm::Value{match.value().position.latitude});
    items.front().insert_or_assign("gpsLongitude", umm::Value{match.value().position.longitude});
    if (match.value().position.altitude_meters) {
      const double alt = *match.value().position.altitude_meters;
      items.front().insert_or_assign("gpsAltitude", umm::Value{std::fabs(alt)});
      items.front().insert_or_assign("gpsAltitudeRef",
                                    umm::Value{std::int64_t{alt < 0 ? 1 : 0}});
    }
    umm::Result<void> set = meta.setLocationCreated(std::move(items));
    if (!set.ok()) {
      reports.push_back({file.string(), false, set.error().message, {}});
      failures.push_back(exit_code_for(set.error().code));
      continue;
    }
    umm::Value gps;
    gps.data = match.value().position;
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("gps", value_to_json(gps));
    report.json_extra.emplace_back("match", Json(match_kind_name(match.value().kind)));
    if (dry) {
      reports.push_back(report);
      if (multi) human += "== " + file.string() + " ==\n";
      human += "GPS  " + value_summary(gps) + "\nMATCH  " + match_kind_name(match.value().kind) + "\n";
      continue;
    }
    umm::Result<umm::WriteReport> wr = umm::write(file, meta, wopts);
    if (!wr.ok()) {
      reports.push_back({file.string(), false, wr.error().message, {}});
      failures.push_back(exit_code_for(wr.error().code));
      continue;
    }
    report.json_extra.emplace_back("report", write_report_json(wr.value()));
    reports.push_back(report);
  }
  emit_mutate(args, reports, failures, files.size(), human, dry, out, err);
  return summarize(failures);
}

bool exiftool_backend_available() {
  const umm::Backend* b = umm::BackendManager::instance().get("exiftool");
  return b && b->availability().available;
}

std::string exiftool_remediation(const ExifToolDiscovery& d, std::string_view reason) {
  std::ostringstream os;
  if (d.path.empty()) {
    os << "ExifTool was not found (discovery: " << discovery_step_name(d.step) << ").\n";
  } else {
    os << "ExifTool is not usable (discovery: " << discovery_step_name(d.step) << ").\n"
       << "  path    " << d.path.string() << "\n";
    if (!reason.empty()) os << "  reason  " << reason << "\n";
    if (exiftool_is_windows_exe(d.path) && reason.find("Perl") != std::string_view::npos)
      os << "Windows ExifTool.exe does not need a separate Perl interpreter.\n";
  }
  os << "Install it with: umm setup exiftool\n"
     << "Discovery order: config file, UMM_EXIFTOOL, PATH.\n";
  return os.str();
}

Json backend_avail_json(const umm::Backend& b) {
  umm::BackendAvailability a = b.availability();
  return Json(Json::Object{{"id", Json(b.id())},
                           {"available", Json(a.available)},
                           {"version", Json(a.version)},
                           {"reason", Json(a.reason)}});
}

ExitCode run_doctor(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  (void)err;
  ConfigParse cfg = load_config(current_platform(), process_env());
  ExifToolDiscovery disc = discover_exiftool(cfg.config, process_env());
  umm::BackendManager& mgr = umm::BackendManager::instance();
  Json::Array backends;
  std::string human = "BACKEND     AVAILABLE  VERSION\n";
  auto pad = [](std::string s, std::size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    return s;
  };
  for (const std::string& id : mgr.backendIds()) {
    const umm::Backend* b = mgr.get(id);
    if (!b) continue;
    umm::BackendAvailability a = b->availability();
    backends.emplace_back(backend_avail_json(*b));
    human += pad(id, 11) + "  " + pad(a.available ? "yes" : "no", 9) + "  " +
             (a.available ? a.version : a.reason) + "\n";
  }
  const umm::Backend* et = mgr.get("exiftool");
  umm::BackendAvailability eta;
  if (et) eta = et->availability();
  Json::Object etj{{"discovery", Json(discovery_step_name(disc.step))},
                   {"path", Json(disc.path.string())},
                   {"version", Json(eta.version)},
                   {"tested_version", Json(UMM_EXIFTOOL_TESTED_VERSION)}};
  if (!disc.perl.empty()) etj.emplace_back("perl", Json(disc.perl.string()));
  human += "\nEXIFTOOL\n";
  human += "  discovery  " + std::string(discovery_step_name(disc.step)) + "\n";
  if (!disc.path.empty()) human += "  path       " + disc.path.string() + "\n";
  if (!eta.version.empty()) human += "  version    " + eta.version + "\n";
  human += "  tested     " + std::string(UMM_EXIFTOOL_TESTED_VERSION) + " (advisory)\n";
  if (!disc.perl.empty()) human += "  perl       " + disc.perl.string() + "\n";
  const bool windows_exe_perl =
      !eta.available && exiftool_is_windows_exe(disc.path) &&
      eta.reason.find("Perl") != std::string::npos;
  if (windows_exe_perl) {
    etj.emplace_back("note", Json("Windows ExifTool.exe does not need a separate Perl interpreter"));
    human += "  note       Windows ExifTool.exe does not need a separate Perl interpreter\n";
  }
  Json::Object extra{{"backends", Json(std::move(backends))}, {"exiftool", Json(std::move(etj))}};
  const bool et_ok = eta.available;
  if (!et_ok) {
    extra.emplace_back("remediation", Json("umm setup exiftool"));
    human += "\nREMEDIATION\n  umm setup exiftool\n";
    Json::Array lost;
    std::string lost_h;
    for (const char* type : {"JPEG", "PNG", "TIFF", "WEBP", "MP4", "MOV", "HEIC", "CR3"}) {
      umm::Result<umm::Capabilities> caps = umm::capabilitiesForType(type);
      if (!caps.ok()) continue;
      bool interesting = caps.value().preferred_backend == "exiftool";
      for (const auto& b : caps.value().backends)
        if (b.backend == "exiv2" && b.identify_only) interesting = true;
      if (!interesting) continue;
      lost.emplace_back(capabilities_json(caps.value()));
      lost_h += format_capabilities(caps.value());
    }
    extra.emplace_back("lost", Json(std::move(lost)));
    if (!lost_h.empty()) human += "\nWITHOUT EXIFTOOL\n" + lost_h;
  }
  if (args.json) {
    out << make_document("doctor", {}, extra).dump() << "\n";
    return ExitCode::ok;
  }
  out << human;
  return ExitCode::ok;
}

std::string shell_quote(const std::string& s) {
#ifdef _WIN32
  std::string o = "\"";
  for (char c : s) {
    if (c == '"') o += '"';
    o += c;
  }
  return o + '"';
#else
  std::string o = "'";
  for (char c : s) {
    if (c == '\'') o += "'\\''";
    else
      o += c;
  }
  return o + "'";
#endif
}

int run_process(const fs::path& program, const std::vector<std::string>& args, std::string* captured) {
  std::string cmd = shell_quote(program.string());
  for (const auto& a : args) cmd += " " + shell_quote(a);
#ifdef _WIN32
  // _popen runs `cmd.exe /c <command>`. With more than two quotes, cmd strips
  // the first and last quote (ERROR_INVALID_NAME). Wrap the whole line.
  cmd.insert(cmd.begin(), '"');
  cmd.push_back('"');
  FILE* f = _popen(cmd.c_str(), "r");
#else
  FILE* f = popen(cmd.c_str(), "r");
#endif
  if (!f) return 127;
  char buf[4096];
  while (std::fgets(buf, sizeof buf, f)) *captured += buf;
#ifdef _WIN32
  return _pclose(f);
#else
  int st = pclose(f);
  if (WIFEXITED(st)) return WEXITSTATUS(st);
  return 1;
#endif
}

std::optional<fs::path> executable_dir() {
#ifdef _WIN32
  wchar_t buf[32768];
  DWORD n = GetModuleFileNameW(nullptr, buf, 32768);
  if (!n || n >= 32768) return std::nullopt;
  return fs::path(buf).parent_path();
#elif defined(__APPLE__)
  char buf[4096];
  uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) != 0) return std::nullopt;
  std::error_code ec;
  fs::path p = fs::weakly_canonical(buf, ec);
  if (ec) p = buf;
  return p.parent_path();
#else
  std::error_code ec;
  fs::path p = fs::read_symlink("/proc/self/exe", ec);
  if (ec) return std::nullopt;
  return p.parent_path();
#endif
}

fs::path find_setup_script() {
#ifdef _WIN32
  const char* name = "exiftool.bat";
#else
  const char* name = "exiftool.sh";
#endif
  std::vector<fs::path> dirs;
  if (auto e = process_env()("UMM_SETUP_SCRIPT_DIR")) dirs.emplace_back(*e);
#ifdef UMM_CLI_SOURCE_DIR
  dirs.emplace_back(fs::path(UMM_CLI_SOURCE_DIR) / "install");
#endif
  if (auto exe = executable_dir()) {
    dirs.push_back(*exe / "install");
    dirs.push_back(exe->parent_path() / "install");
    dirs.push_back(exe->parent_path().parent_path() / "install");
  }
  for (const fs::path& d : dirs) {
    fs::path p = d / name;
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) return fs::weakly_canonical(p, ec);
  }
  return {};
}

ExitCode run_setup(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  if (args.operands.empty() || args.operands[0] != "exiftool") {
    err << "umm setup: unknown target (expected 'exiftool')\n";
    return ExitCode::usage;
  }
  fs::path script = find_setup_script();
  if (script.empty()) {
    err << "umm setup: install script not found\n";
    return ExitCode::io;
  }
  fs::path cfg = primary_config_path(current_platform(), process_env());
  if (cfg.empty()) {
    err << "umm setup: cannot resolve config path\n";
    return ExitCode::usage;
  }
  std::vector<std::string> script_args{"--config", cfg.string()};
  for (std::size_t i = 1; i < args.operands.size(); ++i) script_args.push_back(args.operands[i]);
  std::string captured;
  int rc = run_process(script, script_args, &captured);
  if (rc != 0) {
    err << captured;
    err << "umm setup: install script failed\n";
    return ExitCode::io;
  }
  out << captured;
  return ExitCode::ok;
}

ExitCode run_cast(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  if (args.operands.size() < 2) {
    err << "umm cast: need FILE... and a direction (up, down, or side)\n";
    return ExitCode::usage;
  }
  std::optional<umm::CastDirection> direction = parse_cast_direction(args.operands.back());
  if (!direction) {
    err << "umm cast: unknown direction '" << args.operands.back()
        << "' (expected up, down, or side)\n";
    return ExitCode::usage;
  }
  std::vector<std::string> file_ops(args.operands.begin(), args.operands.end() - 1);
  umm::CastOptions options = cast_options(args);
  std::vector<fs::path> files = expand_operands(file_ops, args.recursive);
  std::vector<FileReport> reports;
  std::vector<ExitCode> failures;
  std::string human;
  const bool multi = files.size() > 1;
  for (const fs::path& file : files) {
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      reports.push_back({file.string(), false, pre.error().message, {}});
      failures.push_back(exit_code_for(pre.error().code));
      continue;
    }
    umm::Result<umm::CastReport> r = umm::cast(file, *direction, options);
    if (!r.ok()) {
      reports.push_back({file.string(), false, r.error().message, {}});
      failures.push_back(exit_code_for(r.error().code));
      continue;
    }
    FileReport report{file.string(), true, "", {}};
    report.json_extra.emplace_back("cast_candidates", cast_candidates_json(r.value().candidates));
    reports.push_back(report);
    if (multi) human += "== " + file.string() + " ==\n";
    human += "CAST CANDIDATES";
    if (options.dry_run) human += "  (preview, not stored)";
    human += "\n";
    human += format_cast_candidates(r.value().candidates);
  }
  emit_inspect(args, reports, failures, files.size(), human, out, err);
  return summarize(failures);
}

ExitCode run_map(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  if (args.operands.empty() || args.operands.size() > 2) {
    err << "umm map: expected PROPERTY [FILE]\n";
    return ExitCode::usage;
  }
  LayerFilter filter;
  auto layers = args.options.find("layers");
  if (layers != args.options.end()) {
    std::string error;
    std::optional<LayerFilter> parsed = parse_layers(layers->second, &error);
    if (!parsed) {
      err << "umm map: " << error << "\n";
      return ExitCode::usage;
    }
    filter = *parsed;
  }
  const std::string& property = args.operands[0];
  umm::Result<umm::PropertyMap> r;
  if (args.operands.size() == 2) {
    fs::path file = args.operands[1];
    umm::Result<void> pre = check_file(file);
    if (!pre.ok()) {
      err << "umm map: " << pre.error().message << "\n";
      return exit_code_for(pre.error().code);
    }
    r = umm::describe(property, file);
  } else {
    r = umm::describe(property);
  }
  if (!r.ok()) {
    err << "umm map: " << r.error().message << "\n";
    return exit_code_for(r.error().code);
  }
  if (args.json) {
    Json payload = property_map_json(r.value(), filter);
    Json::Object extra;
    if (const Json::Object* o = payload.as_object()) extra = *o;
    out << make_document("map", {}, extra).dump() << "\n";
    return ExitCode::ok;
  }
  out << format_property_map(r.value(), filter);
  return ExitCode::ok;
}

}  // namespace

ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  const CommandSpec& cmd = *args.command;
  if (args.backend == "exiftool" && cmd.name != "doctor" && cmd.name != "setup" &&
      cmd.name != "version" && cmd.name != "map") {
    if (!exiftool_backend_available()) {
      ConfigParse cfg = load_config(current_platform(), process_env());
      std::string reason;
      if (const umm::Backend* b = umm::BackendManager::instance().get("exiftool"))
        reason = b->availability().reason;
      err << exiftool_remediation(discover_exiftool(cfg.config, process_env()), reason);
      return ExitCode::backend;
    }
  }
  if (cmd.name == "version") return run_version(args, out);
  if (cmd.name == "read") return run_read(args, out, err);
  if (cmd.name == "get") return run_get(args, out, err);
  if (cmd.name == "set") return run_set(args, out, err);
  if (cmd.name == "rm") return run_rm(args, out, err);
  if (cmd.name == "dumpall") return run_dump(args, false, out, err);
  if (cmd.name == "dumpunmapped") return run_dump(args, true, out, err);
  if (cmd.name == "conflicts") return run_conflicts(args, out, err);
  if (cmd.name == "merge") return run_merge(args, out, err);
  if (cmd.name == "sync") return run_sync(args, out, err);
  if (cmd.name == "cast") return run_cast(args, out, err);
  if (cmd.name == "caps") return run_caps(args, out, err);
  if (cmd.name == "map") return run_map(args, out, err);
  if (cmd.name == "geotag") return run_geotag(args, out, err);
  if (cmd.name == "doctor") return run_doctor(args, out, err);
  if (cmd.name == "setup") return run_setup(args, out, err);

  err << "umm " << cmd.name << ": not implemented yet\n";
  return ExitCode::not_implemented;
}

}  // namespace umm_cli
