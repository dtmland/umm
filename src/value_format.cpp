#include "value_format.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

namespace umm_cli {

namespace {

std::string pad2(int value) {
  const int abs_value = value < 0 ? -value : value;
  std::string digits = std::to_string(abs_value);
  if (digits.size() < 2) digits.insert(digits.begin(), 2 - digits.size(), '0');
  if (value < 0) digits.insert(digits.begin(), '-');
  return digits;
}

std::string datetime_iso(const umm::DateTime& dt) {
  std::string out = std::to_string(dt.year);
  if (!dt.month) return out;
  out += '-';
  out += pad2(*dt.month);
  if (!dt.day) return out;
  out += '-';
  out += pad2(*dt.day);
  if (!dt.hour) return out;
  out += 'T';
  out += pad2(*dt.hour);
  out += ':';
  out += pad2(dt.minute.value_or(0));
  if (dt.second) {
    out += ':';
    out += pad2(*dt.second);
    if (dt.subsecond_ns) {
      std::string ns = std::to_string(*dt.subsecond_ns);
      if (ns.size() < 9) ns.insert(ns.begin(), 9 - ns.size(), '0');
      out += '.';
      out += ns;
    }
  }
  if (dt.utc_offset_minutes) {
    int off = *dt.utc_offset_minutes;
    if (off == 0) {
      out += 'Z';
    } else {
      const char sign = off < 0 ? '-' : '+';
      if (off < 0) off = -off;
      out += sign;
      out += pad2(off / 60);
      out += ':';
      out += pad2(off % 60);
    }
  }
  return out;
}

Json structure_to_json(const umm::Structure& fields) {
  Json::Object o;
  for (const auto& [k, v] : fields) o.emplace_back(k, value_to_json(v));
  return Json(std::move(o));
}

std::string structure_label(const umm::Structure& fields) {
  for (const char* key : {"name", "city"}) {
    auto it = fields.find(key);
    if (it == fields.end()) continue;
    std::string n = value_summary(it->second);
    if (!n.empty()) return n;
  }
  return {};
}

}  // namespace

Json value_to_json(const umm::Value& value) {
  return std::visit(
      [](const auto& alt) -> Json {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return Json(alt);
        } else if constexpr (std::is_same_v<T, umm::LangAlt>) {
          Json::Object o;
          for (const auto& [lang, text] : alt) o.emplace_back(lang, Json(text));
          return Json(std::move(o));
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
          Json::Array a;
          for (const auto& s : alt) a.emplace_back(Json(s));
          return Json(std::move(a));
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return Json(static_cast<double>(alt));
        } else if constexpr (std::is_same_v<T, double>) {
          return Json(alt);
        } else if constexpr (std::is_same_v<T, bool>) {
          return Json(alt);
        } else if constexpr (std::is_same_v<T, umm::Rational>) {
          return Json(Json::Object{{"numerator", Json(static_cast<double>(alt.numerator))},
                                   {"denominator", Json(static_cast<double>(alt.denominator))}});
        } else if constexpr (std::is_same_v<T, umm::DateTime>) {
          return Json(datetime_iso(alt));
        } else if constexpr (std::is_same_v<T, umm::GpsCoordinate>) {
          Json::Object o{{"latitude", Json(alt.latitude)}, {"longitude", Json(alt.longitude)}};
          if (alt.altitude_meters) o.emplace_back("altitude_meters", Json(*alt.altitude_meters));
          if (alt.gps_time) o.emplace_back("gps_time", Json(datetime_iso(*alt.gps_time)));
          return Json(std::move(o));
        } else if constexpr (std::is_same_v<T, umm::Structure>) {
          return structure_to_json(alt);
        } else if constexpr (std::is_same_v<T, std::vector<umm::Structure>>) {
          Json::Array a;
          for (const auto& s : alt) a.emplace_back(structure_to_json(s));
          return Json(std::move(a));
        }
        return Json();
      },
      value.data);
}

std::string value_summary(const umm::Value& value) {
  return std::visit(
      [](const auto& alt) -> std::string {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return alt;
        } else if constexpr (std::is_same_v<T, umm::LangAlt>) {
          auto it = alt.find("x-default");
          if (it != alt.end()) return it->second;
          if (!alt.empty()) return alt.begin()->second;
          return {};
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
          std::string out;
          for (std::size_t i = 0; i < alt.size(); ++i) {
            if (i) out += ", ";
            out += alt[i];
          }
          return out;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return std::to_string(alt);
        } else if constexpr (std::is_same_v<T, double>) {
          std::ostringstream os;
          os << alt;
          return os.str();
        } else if constexpr (std::is_same_v<T, bool>) {
          return alt ? "true" : "false";
        } else if constexpr (std::is_same_v<T, umm::Rational>) {
          return std::to_string(alt.numerator) + "/" + std::to_string(alt.denominator);
        } else if constexpr (std::is_same_v<T, umm::DateTime>) {
          return datetime_iso(alt);
        } else if constexpr (std::is_same_v<T, umm::GpsCoordinate>) {
          std::ostringstream os;
          os << alt.latitude << ',' << alt.longitude;
          return os.str();
        } else if constexpr (std::is_same_v<T, umm::Structure>) {
          std::string n = structure_label(alt);
          if (!n.empty()) return n;
          return std::to_string(alt.size()) + (alt.size() == 1 ? " field" : " fields");
        } else if constexpr (std::is_same_v<T, std::vector<umm::Structure>>) {
          std::string out;
          for (const auto& s : alt) {
            std::string n = structure_label(s);
            if (n.empty()) continue;
            if (!out.empty()) out += ", ";
            out += n;
          }
          if (!out.empty()) return out;
          return std::to_string(alt.size()) + (alt.size() == 1 ? " entry" : " entries");
        }
        return {};
      },
      value.data);
}

const char* resolution_name(umm::Resolution resolution) noexcept {
  switch (resolution) {
    case umm::Resolution::single:
      return "single";
    case umm::Resolution::equivalent:
      return "equivalent";
    case umm::Resolution::reconciled:
      return "reconciled";
    case umm::Resolution::conflict:
      return "conflict";
  }
  return "single";
}

PropertyRow property_row(std::string id, const umm::PropertyValue& property, bool sources) {
  PropertyRow row;
  row.id = std::move(id);
  row.value = value_to_json(property.value);
  row.summary = value_summary(property.value);
  if (sources) {
    std::string keys;
    Json::Array srcs;
    for (std::size_t i = 0; i < property.sources.size(); ++i) {
      if (i) keys += ",";
      keys += property.sources[i].base_key;
      srcs.emplace_back(Json(Json::Object{{"base_key", Json(property.sources[i].base_key)},
                                          {"backend", Json(property.sources[i].backend)},
                                          {"container", Json(property.sources[i].container)}}));
    }
    row.extra.push_back(std::move(keys));
    row.extra.push_back(resolution_name(property.resolution));
    row.json_extra.emplace_back("resolution", Json(resolution_name(property.resolution)));
    row.json_extra.emplace_back("sources", Json(std::move(srcs)));
    if (!property.preferred_source.empty())
      row.json_extra.emplace_back("preferred_source", Json(property.preferred_source));
  }
  return row;
}

namespace {

umm::Error bad_value(std::string_view why) {
  return umm::Error{umm::ErrorCode::invalid_value, std::string(why), "", ""};
}

umm::Value make_value(auto payload) {
  umm::Value v;
  v.data = std::move(payload);
  return v;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

std::string unquote(std::string_view s) {
  s = trim(s);
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
    s = s.substr(1, s.size() - 2);
  return std::string(s);
}

std::vector<std::string> split_csv(std::string_view s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      out.push_back(unquote(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  out.push_back(unquote(cur));
  return out;
}

std::optional<int> parse_int_digits(std::string_view s) {
  if (s.empty()) return std::nullopt;
  std::size_t i = 0;
  if (s[0] == '-' || s[0] == '+') ++i;
  if (i >= s.size()) return std::nullopt;
  for (; i < s.size(); ++i)
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
  try {
    return std::stoi(std::string(s));
  } catch (...) {
    return std::nullopt;
  }
}

umm::Result<umm::DateTime> parse_datetime(std::string_view text) {
  std::string s = unquote(text);
  umm::DateTime dt;
  auto take = [&](std::size_t n) -> std::optional<int> {
    if (s.size() < n) return std::nullopt;
    auto v = parse_int_digits(s.substr(0, n));
    if (!v) return std::nullopt;
    s.erase(0, n);
    return v;
  };
  auto year = take(4);
  if (!year) return bad_value("invalid date-time '" + unquote(text) + "'");
  dt.year = *year;
  if (s.empty()) return dt;
  if (s[0] != '-') return bad_value("invalid date-time '" + unquote(text) + "'");
  s.erase(0, 1);
  auto month = take(2);
  if (!month) return bad_value("invalid date-time '" + unquote(text) + "'");
  dt.month = *month;
  if (s.empty()) return dt;
  if (s[0] != '-') return bad_value("invalid date-time '" + unquote(text) + "'");
  s.erase(0, 1);
  auto day = take(2);
  if (!day) return bad_value("invalid date-time '" + unquote(text) + "'");
  dt.day = *day;
  if (s.empty()) return dt;
  if (s[0] == 'T' || s[0] == 't' || s[0] == ' ')
    s.erase(0, 1);
  else
    return bad_value("invalid date-time '" + unquote(text) + "'");
  auto hour = take(2);
  if (!hour) return bad_value("invalid date-time '" + unquote(text) + "'");
  dt.hour = *hour;
  if (s.empty() || s[0] != ':') return bad_value("invalid date-time '" + unquote(text) + "'");
  s.erase(0, 1);
  auto minute = take(2);
  if (!minute) return bad_value("invalid date-time '" + unquote(text) + "'");
  dt.minute = *minute;
  if (!s.empty() && s[0] == ':') {
    s.erase(0, 1);
    auto second = take(2);
    if (!second) return bad_value("invalid date-time '" + unquote(text) + "'");
    dt.second = *second;
    if (!s.empty() && s[0] == '.') {
      s.erase(0, 1);
      std::size_t n = 0;
      while (n < s.size() && std::isdigit(static_cast<unsigned char>(s[n]))) ++n;
      std::string frac(s.substr(0, n));
      s.erase(0, n);
      while (frac.size() < 9) frac.push_back('0');
      if (frac.size() > 9) frac.resize(9);
      dt.subsecond_ns = std::stoi(frac);
    }
  }
  if (!s.empty()) {
    if (s[0] == 'Z' || s[0] == 'z') {
      if (s.size() != 1) return bad_value("invalid date-time '" + unquote(text) + "'");
      dt.utc_offset_minutes = 0;
    } else if (s[0] == '+' || s[0] == '-') {
      const int sign = s[0] == '-' ? -1 : 1;
      s.erase(0, 1);
      auto oh = take(2);
      if (!oh) return bad_value("invalid date-time '" + unquote(text) + "'");
      int om = 0;
      if (!s.empty() && s[0] == ':') {
        s.erase(0, 1);
        auto omin = take(2);
        if (!omin) return bad_value("invalid date-time '" + unquote(text) + "'");
        om = *omin;
      } else if (s.size() >= 2) {
        auto omin = take(2);
        if (!omin) return bad_value("invalid date-time '" + unquote(text) + "'");
        om = *omin;
      }
      if (!s.empty()) return bad_value("invalid date-time '" + unquote(text) + "'");
      dt.utc_offset_minutes = sign * (*oh * 60 + om);
    } else {
      return bad_value("invalid date-time '" + unquote(text) + "'");
    }
  }
  return dt;
}

umm::Result<umm::Value> json_to_value(const Json& j, umm::Datatype dt);

umm::Result<umm::Value> json_to_untyped(const Json& j) {
  if (j.is_null()) return bad_value("null is not a metadata value");
  if (const bool* b = j.as_bool()) return make_value(*b);
  if (const double* n = j.as_number()) {
    if (std::isfinite(*n) && *n == std::floor(*n) && std::fabs(*n) < 1e15)
      return make_value(static_cast<std::int64_t>(*n));
    return make_value(*n);
  }
  if (const std::string* s = j.as_string()) return make_value(*s);
  if (const Json::Object* o = j.as_object()) {
    umm::Structure fields;
    for (const auto& [k, v] : *o) {
      umm::Result<umm::Value> inner = json_to_untyped(v);
      if (!inner.ok()) return inner;
      fields.emplace(k, inner.value());
    }
    return make_value(std::move(fields));
  }
  if (const Json::Array* a = j.as_array()) {
    bool all_str = true, all_obj = true;
    for (const Json& e : *a) {
      if (!e.as_string()) all_str = false;
      if (!e.as_object()) all_obj = false;
    }
    if (all_str) {
      std::vector<std::string> xs;
      for (const Json& e : *a) xs.push_back(*e.as_string());
      return make_value(std::move(xs));
    }
    if (all_obj) {
      std::vector<umm::Structure> xs;
      for (const Json& e : *a) {
        umm::Result<umm::Value> inner = json_to_untyped(e);
        if (!inner.ok()) return inner;
        if (const auto* st = std::get_if<umm::Structure>(&inner.value().data))
          xs.push_back(*st);
        else
          return bad_value("expected object in array");
      }
      return make_value(std::move(xs));
    }
    return bad_value("unsupported JSON array");
  }
  return bad_value("unsupported JSON value");
}

umm::Result<umm::Value> json_to_value(const Json& j, umm::Datatype dt) {
  switch (dt) {
    case umm::Datatype::text: {
      if (const std::string* s = j.as_string()) return make_value(*s);
      return bad_value("expected JSON string");
    }
    case umm::Datatype::lang_alt: {
      if (const std::string* s = j.as_string()) return make_value(umm::LangAlt{{"x-default", *s}});
      const Json::Object* o = j.as_object();
      if (!o) return bad_value("expected JSON object for lang-alt");
      umm::LangAlt alt;
      for (const auto& [k, v] : *o) {
        const std::string* t = v.as_string();
        if (!t) return bad_value("lang-alt values must be strings");
        alt.emplace(k, *t);
      }
      return make_value(std::move(alt));
    }
    case umm::Datatype::text_list: {
      if (const std::string* s = j.as_string()) return make_value(split_csv(*s));
      const Json::Array* a = j.as_array();
      if (!a) return bad_value("expected JSON array of strings");
      std::vector<std::string> xs;
      for (const Json& e : *a) {
        const std::string* t = e.as_string();
        if (!t) return bad_value("expected strings in array");
        xs.push_back(*t);
      }
      return make_value(std::move(xs));
    }
    case umm::Datatype::integer: {
      const double* n = j.as_number();
      if (!n || *n != std::floor(*n)) return bad_value("expected JSON integer");
      return make_value(static_cast<std::int64_t>(*n));
    }
    case umm::Datatype::real: {
      if (const double* n = j.as_number()) return make_value(*n);
      return bad_value("expected JSON number");
    }
    case umm::Datatype::boolean: {
      if (const bool* b = j.as_bool()) return make_value(*b);
      return bad_value("expected JSON boolean");
    }
    case umm::Datatype::rational: {
      const Json::Object* o = j.as_object();
      if (!o) return bad_value("expected JSON object {numerator,denominator}");
      umm::Rational r;
      bool have_n = false, have_d = false;
      for (const auto& [k, v] : *o) {
        const double* n = v.as_number();
        if (!n) return bad_value("rational fields must be numbers");
        if (k == "numerator") {
          r.numerator = static_cast<std::int64_t>(*n);
          have_n = true;
        } else if (k == "denominator") {
          r.denominator = static_cast<std::int64_t>(*n);
          have_d = true;
        }
      }
      if (!have_n || !have_d) return bad_value("rational needs numerator and denominator");
      return make_value(r);
    }
    case umm::Datatype::date_time: {
      const std::string* s = j.as_string();
      if (!s) return bad_value("expected ISO-8601 string");
      umm::Result<umm::DateTime> dt = parse_datetime(*s);
      if (!dt.ok()) return dt.error();
      return make_value(dt.value());
    }
    case umm::Datatype::structure: {
      if (j.as_array() && j.as_array()->size() == 1)
        return json_to_value(j.as_array()->front(), umm::Datatype::structure);
      umm::Result<umm::Value> raw = json_to_untyped(j);
      if (!raw.ok()) return raw;
      if (!std::holds_alternative<umm::Structure>(raw.value().data))
        return bad_value("expected JSON object");
      return raw;
    }
    case umm::Datatype::structure_list: {
      if (j.as_object()) {
        umm::Result<umm::Value> one = json_to_value(j, umm::Datatype::structure);
        if (!one.ok()) return one;
        return make_value(std::vector<umm::Structure>{std::get<umm::Structure>(one.value().data)});
      }
      const Json::Array* a = j.as_array();
      if (!a) return bad_value("expected JSON array of objects");
      std::vector<umm::Structure> xs;
      for (const Json& e : *a) {
        umm::Result<umm::Value> one = json_to_value(e, umm::Datatype::structure);
        if (!one.ok()) return one;
        xs.push_back(std::get<umm::Structure>(one.value().data));
      }
      return make_value(std::move(xs));
    }
  }
  return bad_value("unknown datatype");
}

}  // namespace

umm::Result<umm::Value> parse_value(umm::Datatype datatype, std::string_view text, bool json) {
  if (json) {
    Json j;
    std::string err;
    if (!Json::parse(text, j, &err))
      return bad_value(err.empty() ? "invalid JSON" : err);
    return json_to_value(j, datatype);
  }
  switch (datatype) {
    case umm::Datatype::text:
      return make_value(unquote(text));
    case umm::Datatype::lang_alt:
      return make_value(umm::LangAlt{{"x-default", unquote(text)}});
    case umm::Datatype::text_list:
      return make_value(split_csv(text));
    case umm::Datatype::integer:
      try {
        return make_value(static_cast<std::int64_t>(std::stoll(unquote(text))));
      } catch (...) {
        return bad_value("invalid integer");
      }
    case umm::Datatype::real:
      try {
        return make_value(std::stod(unquote(text)));
      } catch (...) {
        return bad_value("invalid number");
      }
    case umm::Datatype::boolean: {
      std::string s = unquote(text);
      if (s == "true" || s == "1") return make_value(true);
      if (s == "false" || s == "0") return make_value(false);
      return bad_value("invalid boolean");
    }
    case umm::Datatype::rational: {
      std::string s = unquote(text);
      auto slash = s.find('/');
      if (slash == std::string::npos) return bad_value("rational must be n/d");
      try {
        umm::Rational r;
        r.numerator = static_cast<std::int64_t>(std::stoll(s.substr(0, slash)));
        r.denominator = static_cast<std::int64_t>(std::stoll(s.substr(slash + 1)));
        return make_value(r);
      } catch (...) {
        return bad_value("invalid rational");
      }
    }
    case umm::Datatype::date_time: {
      umm::Result<umm::DateTime> dt = parse_datetime(text);
      if (!dt.ok()) return dt.error();
      return make_value(dt.value());
    }
    case umm::Datatype::structure:
    case umm::Datatype::structure_list:
      return bad_value("struct values require --json");
  }
  return bad_value("unknown datatype");
}

}  // namespace umm_cli
