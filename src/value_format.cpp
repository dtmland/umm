#include "value_format.hpp"

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
          return std::to_string(alt.size()) + (alt.size() == 1 ? " field" : " fields");
        } else if constexpr (std::is_same_v<T, std::vector<umm::Structure>>) {
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
      keys += property.sources[i].raw_key;
      srcs.emplace_back(Json(Json::Object{{"raw_key", Json(property.sources[i].raw_key)},
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

}  // namespace umm_cli
