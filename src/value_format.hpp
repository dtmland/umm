// Present libumm values: compact human summary and JSON (concept §2.5).
#pragma once

#include <string>

#include "output.hpp"
#include "umm/provenance.hpp"
#include "umm/value.hpp"

namespace umm_cli {

Json value_to_json(const umm::Value& value);
std::string value_summary(const umm::Value& value);
const char* resolution_name(umm::Resolution resolution) noexcept;

// One table/JSON row. When `sources` is true, extra human columns are SOURCE
// and RESOLUTION, and JSON gains sources/resolution(/preferred_source).
PropertyRow property_row(std::string id, const umm::PropertyValue& property, bool sources);

}  // namespace umm_cli
