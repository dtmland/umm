#include "property.hpp"

namespace umm_cli {

umm::Result<ResolvedProperty> resolve_property(std::string_view name, umm::MediaDomain) {
  if (name.find('.') != std::string_view::npos) {
    if (umm::registry().find(name)) return ResolvedProperty{std::string(name), std::string(name)};
  }
  return umm::Error{umm::ErrorCode::unknown_property, "unknown property '" + std::string(name) + "'", "", ""};
}

}  // namespace umm_cli
