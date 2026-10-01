#include "output.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace umm_cli {

std::string json_escape(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

void Json::write(std::string& out, int indent, int depth) const {
  auto newline = [&](int d) {
    if (indent < 0) return;
    out += '\n';
    out.append(static_cast<std::size_t>(indent * d), ' ');
  };
  if (std::holds_alternative<std::nullptr_t>(v_)) {
    out += "null";
  } else if (auto* b = std::get_if<bool>(&v_)) {
    out += *b ? "true" : "false";
  } else if (auto* n = std::get_if<double>(&v_)) {
    char buf[40];
    if (std::isfinite(*n) && *n == std::floor(*n) && std::fabs(*n) < 1e15)
      std::snprintf(buf, sizeof buf, "%.0f", *n);
    else if (std::isfinite(*n))
      std::snprintf(buf, sizeof buf, "%.17g", *n);
    else
      std::snprintf(buf, sizeof buf, "null");
    out += buf;
  } else if (auto* s = std::get_if<std::string>(&v_)) {
    out += '"' + json_escape(*s) + '"';
  } else if (auto* a = std::get_if<Array>(&v_)) {
    if (a->empty()) { out += "[]"; return; }
    out += '[';
    for (std::size_t i = 0; i < a->size(); ++i) {
      if (i) out += ',';
      newline(depth + 1);
      (*a)[i].write(out, indent, depth + 1);
    }
    newline(depth);
    out += ']';
  } else if (auto* o = std::get_if<Object>(&v_)) {
    if (o->empty()) { out += "{}"; return; }
    out += '{';
    for (std::size_t i = 0; i < o->size(); ++i) {
      if (i) out += ',';
      newline(depth + 1);
      out += '"' + json_escape((*o)[i].first) + "\":";
      if (indent >= 0) out += ' ';
      (*o)[i].second.write(out, indent, depth + 1);
    }
    newline(depth);
    out += '}';
  }
}

std::string Json::dump(int indent) const {
  std::string out;
  write(out, indent, 0);
  return out;
}

std::string format_table(const std::vector<FileReport>& files, const std::vector<std::string>& extra_headers) {
  std::string out;
  const bool headers = files.size() > 1;
  for (const FileReport& f : files) {
    if (headers) out += "== " + f.path + " ==\n";
    if (!f.ok) {
      out += "error: " + f.error + "\n";
      continue;
    }
    std::size_t idw = 8, valw = 5;
    for (const PropertyRow& p : f.properties) {
      idw = std::max(idw, p.id.size());
      valw = std::max(valw, p.summary.size());
    }
    auto pad = [](std::string s, std::size_t w) {
      if (s.size() < w) s.append(w - s.size(), ' ');
      return s;
    };
    std::string head = pad("PROPERTY", idw) + "  " + pad("VALUE", valw);
    for (const std::string& h : extra_headers) head += "  " + h;
    out += head + "\n";
    for (const PropertyRow& p : f.properties) {
      std::string line = pad(p.id, idw) + "  " + pad(p.summary, valw);
      for (const std::string& e : p.extra) line += "  " + e;
      while (!line.empty() && line.back() == ' ') line.pop_back();
      out += line + "\n";
    }
  }
  return out;
}

std::string format_values(const std::vector<FileReport>& files) {
  std::string out;
  const bool headers = files.size() > 1;
  for (const FileReport& f : files) {
    if (headers) out += "== " + f.path + " ==\n";
    if (!f.ok) {
      out += "error: " + f.error + "\n";
      continue;
    }
    for (const PropertyRow& p : f.properties) out += p.summary + "\n";
  }
  return out;
}

Json make_document(const std::string& command, const std::vector<FileReport>& files, Json::Object extra) {
  Json::Object doc;
  doc.emplace_back("schema_version", Json(kSchemaVersion));
  doc.emplace_back("command", Json(command));
  for (auto& kv : extra) doc.push_back(std::move(kv));
  Json::Array arr;
  for (const FileReport& f : files) {
    Json::Object fo;
    fo.emplace_back("path", Json(f.path));
    fo.emplace_back("ok", Json(f.ok));
    if (!f.ok) fo.emplace_back("error", Json(f.error));
    if (f.ok || !f.properties.empty()) {
      Json::Array props;
      for (const PropertyRow& p : f.properties) {
        Json::Object po;
        po.emplace_back("id", Json(p.id));
        po.emplace_back("value", p.value);
        for (const auto& kv : p.json_extra) po.push_back(kv);
        props.emplace_back(std::move(po));
      }
      fo.emplace_back("properties", Json(std::move(props)));
    }
    for (const auto& kv : f.json_extra) fo.push_back(kv);
    arr.emplace_back(std::move(fo));
  }
  if (!files.empty()) doc.emplace_back("files", Json(std::move(arr)));
  return Json(std::move(doc));
}

}  // namespace umm_cli
