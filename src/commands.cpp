#include "commands.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ostream>
#include <sstream>

#include "batch.hpp"
#include "output.hpp"
#include "property.hpp"
#include "umm/umm.hpp"
#include "value_format.hpp"

#ifndef UMM_CLI_VERSION
#define UMM_CLI_VERSION "0.1.0"
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
  return options;
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
    sw = std::max(sw, s.standard.size());
    vw = std::max(vw, s.version.size());
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
  std::vector<fs::path> files = expand_operands(args.operands, args.recursive);
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
    for (const std::string& id : r.value().propertyIds()) {
      std::optional<umm::PropertyValue> pv = r.value().get(id);
      if (pv) report.properties.push_back(property_row(id, *pv, sources));
    }
    reports.push_back(std::move(report));
  }
  emit_reports(args, reports, failures, files.size(), out, err,
               sources ? std::vector<std::string>{"SOURCE", "RESOLUTION"} : std::vector<std::string>{},
               false);
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

}  // namespace

ExitCode run_command(const ParsedArgs& args, std::ostream& out, std::ostream& err) {
  const CommandSpec& cmd = *args.command;
  if (cmd.name == "version") return run_version(args, out);
  if (cmd.name == "read") return run_read(args, out, err);
  if (cmd.name == "get") return run_get(args, out, err);

  if (cmd.batch_files) {
    std::vector<std::string> file_operands = args.operands;
    if (cmd.operands == Operands::file_then_args) file_operands.resize(1);
    BatchSummary summary = run_batch(expand_operands(file_operands, args.recursive), check_file);
    if (!summary.failures.empty()) {
      if (args.json) {
        std::vector<FileReport> reports;
        for (const FileFailure& f : summary.failures)
          reports.push_back({f.path.string(), false, f.message, {}});
        out << make_document(std::string(cmd.name), reports).dump() << "\n";
      } else {
        for (const FileFailure& f : summary.failures)
          err << "umm " << cmd.name << ": " << f.message << " [" << group_name(f.code) << "]\n";
        err << "umm " << cmd.name << ": " << summary.failures.size() << " of " << summary.total
            << " file(s) failed\n";
      }
      return summary.exit_code();
    }
  }

  err << "umm " << cmd.name << ": not implemented yet\n";
  return ExitCode::not_implemented;
}

}  // namespace umm_cli
