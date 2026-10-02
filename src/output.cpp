#include "output.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>

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

namespace {

struct JsonParser {
  std::string_view in;
  std::size_t i{0};
  std::string* error{nullptr};

  void fail(const char* msg) {
    if (error && error->empty()) *error = msg;
  }
  void skip() {
    while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i]))) ++i;
  }
  bool eat(char c) {
    skip();
    if (i < in.size() && in[i] == c) {
      ++i;
      return true;
    }
    return false;
  }
  bool parse_value(Json& out) {
    skip();
    if (i >= in.size()) {
      fail("unexpected end of JSON");
      return false;
    }
    const char c = in[i];
    if (c == 'n') return parse_lit("null", Json(), out);
    if (c == 't') return parse_lit("true", Json(true), out);
    if (c == 'f') return parse_lit("false", Json(false), out);
    if (c == '"') {
      std::string s;
      if (!parse_string(s)) return false;
      out = Json(std::move(s));
      return true;
    }
    if (c == '{') return parse_object(out);
    if (c == '[') return parse_array(out);
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(out);
    fail("unexpected JSON token");
    return false;
  }
  bool parse_lit(const char* lit, Json value, Json& out) {
    const std::size_t n = std::char_traits<char>::length(lit);
    if (in.substr(i, n) != std::string_view(lit, n)) {
      fail("invalid JSON literal");
      return false;
    }
    i += n;
    out = std::move(value);
    return true;
  }
  bool parse_string(std::string& s) {
    if (!eat('"')) {
      fail("expected string");
      return false;
    }
    while (i < in.size()) {
      char c = in[i++];
      if (c == '"') return true;
      if (c == '\\') {
        if (i >= in.size()) {
          fail("unterminated string escape");
          return false;
        }
        char e = in[i++];
        switch (e) {
          case '"':
          case '\\':
          case '/':
            s += e;
            break;
          case 'b':
            s += '\b';
            break;
          case 'f':
            s += '\f';
            break;
          case 'n':
            s += '\n';
            break;
          case 'r':
            s += '\r';
            break;
          case 't':
            s += '\t';
            break;
          case 'u': {
            if (i + 4 > in.size()) {
              fail("invalid unicode escape");
              return false;
            }
            unsigned code = 0;
            for (int k = 0; k < 4; ++k) {
              char h = in[i++];
              code <<= 4;
              if (h >= '0' && h <= '9')
                code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f')
                code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F')
                code |= static_cast<unsigned>(h - 'A' + 10);
              else {
                fail("invalid unicode escape");
                return false;
              }
            }
            if (code < 0x80)
              s += static_cast<char>(code);
            else if (code < 0x800) {
              s += static_cast<char>(0xc0 | (code >> 6));
              s += static_cast<char>(0x80 | (code & 0x3f));
            } else {
              s += static_cast<char>(0xe0 | (code >> 12));
              s += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
              s += static_cast<char>(0x80 | (code & 0x3f));
            }
            break;
          }
          default:
            fail("invalid string escape");
            return false;
        }
      } else if (static_cast<unsigned char>(c) < 0x20) {
        fail("unescaped control in string");
        return false;
      } else {
        s += c;
      }
    }
    fail("unterminated string");
    return false;
  }
  bool parse_number(Json& out) {
    const std::size_t start = i;
    if (in[i] == '-') ++i;
    if (i >= in.size() || !std::isdigit(static_cast<unsigned char>(in[i]))) {
      fail("invalid number");
      return false;
    }
    if (in[i] == '0') {
      ++i;
    } else {
      while (i < in.size() && std::isdigit(static_cast<unsigned char>(in[i]))) ++i;
    }
    if (i < in.size() && in[i] == '.') {
      ++i;
      if (i >= in.size() || !std::isdigit(static_cast<unsigned char>(in[i]))) {
        fail("invalid number");
        return false;
      }
      while (i < in.size() && std::isdigit(static_cast<unsigned char>(in[i]))) ++i;
    }
    if (i < in.size() && (in[i] == 'e' || in[i] == 'E')) {
      ++i;
      if (i < in.size() && (in[i] == '+' || in[i] == '-')) ++i;
      if (i >= in.size() || !std::isdigit(static_cast<unsigned char>(in[i]))) {
        fail("invalid number");
        return false;
      }
      while (i < in.size() && std::isdigit(static_cast<unsigned char>(in[i]))) ++i;
    }
    std::string tok(in.substr(start, i - start));
    char* end = nullptr;
    double n = std::strtod(tok.c_str(), &end);
    if (!end || end != tok.c_str() + tok.size()) {
      fail("invalid number");
      return false;
    }
    out = Json(n);
    return true;
  }
  bool parse_object(Json& out) {
    if (!eat('{')) return false;
    Json::Object obj;
    skip();
    if (eat('}')) {
      out = Json(std::move(obj));
      return true;
    }
    for (;;) {
      skip();
      std::string key;
      if (!parse_string(key)) return false;
      if (!eat(':')) {
        fail("expected ':'");
        return false;
      }
      Json val;
      if (!parse_value(val)) return false;
      obj.emplace_back(std::move(key), std::move(val));
      skip();
      if (eat('}')) {
        out = Json(std::move(obj));
        return true;
      }
      if (!eat(',')) {
        fail("expected ',' or '}'");
        return false;
      }
    }
  }
  bool parse_array(Json& out) {
    if (!eat('[')) return false;
    Json::Array arr;
    skip();
    if (eat(']')) {
      out = Json(std::move(arr));
      return true;
    }
    for (;;) {
      Json val;
      if (!parse_value(val)) return false;
      arr.push_back(std::move(val));
      skip();
      if (eat(']')) {
        out = Json(std::move(arr));
        return true;
      }
      if (!eat(',')) {
        fail("expected ',' or ']'");
        return false;
      }
    }
  }
};

}  // namespace

bool Json::parse(std::string_view text, Json& out, std::string* error) {
  if (error) error->clear();
  JsonParser p{text, 0, error};
  if (!p.parse_value(out)) return false;
  p.skip();
  if (p.i != text.size()) {
    if (error && error->empty()) *error = "trailing JSON text";
    return false;
  }
  return true;
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
      if (p.id.size() > idw) idw = p.id.size();
      if (p.summary.size() > valw) valw = p.summary.size();
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
    // Always emit properties for a successful dump with no extra payload
    // (e.g. empty `umm read`). Inspect commands attach json_extra instead.
    if (!f.properties.empty() || (f.ok && f.json_extra.empty())) {
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
