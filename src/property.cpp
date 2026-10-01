#include "property.hpp"

namespace umm_cli {

namespace {

// One entry per public `std::optional<PropertyValue> NAME() const` getter in
// libumm's include/umm/metadata.hpp (v0.1.0). The binding is to libumm's own
// member functions; ids and domain resolution stay in libumm.
struct Binding {
  std::string_view name;
  AccessorGetter getter;
};

const Binding kBindings[] = {
    {"creator", &umm::Metadata::creator},
    {"description", &umm::Metadata::description},
    {"headline", &umm::Metadata::headline},
    {"dateCreated", &umm::Metadata::dateCreated},
    {"copyrightNotice", &umm::Metadata::copyrightNotice},
    {"creditLine", &umm::Metadata::creditLine},
    {"keywords", &umm::Metadata::keywords},
    {"rating", &umm::Metadata::rating},
    {"title", &umm::Metadata::title},
    {"altTextAccessibility", &umm::Metadata::altTextAccessibility},
    {"extendedDescriptionAccessibility", &umm::Metadata::extendedDescriptionAccessibility},
    {"rightsUsageTerms", &umm::Metadata::rightsUsageTerms},
    {"sourceSupplyChain", &umm::Metadata::sourceSupplyChain},
    {"dataMining", &umm::Metadata::dataMining},
    {"contributor", &umm::Metadata::contributor},
    {"genre", &umm::Metadata::genre},
    {"embeddedEncodedRightsExpression", &umm::Metadata::embeddedEncodedRightsExpression},
    {"linkedEncodedRightsExpression", &umm::Metadata::linkedEncodedRightsExpression},
    {"aiPromptInformation", &umm::Metadata::aiPromptInformation},
    {"aiPromptWriterName", &umm::Metadata::aiPromptWriterName},
    {"aiSystemUsed", &umm::Metadata::aiSystemUsed},
    {"aiSystemVersionUsed", &umm::Metadata::aiSystemVersionUsed},
    {"otherConstraints", &umm::Metadata::otherConstraints},
    {"digitalSourceType", &umm::Metadata::digitalSourceType},
    {"modelReleaseStatus", &umm::Metadata::modelReleaseStatus},
    {"propertyReleaseStatus", &umm::Metadata::propertyReleaseStatus},
    {"copyrightOwner", &umm::Metadata::copyrightOwner},
    {"licensor", &umm::Metadata::licensor},
    {"gps", &umm::Metadata::gps},
    {"locationCreated", &umm::Metadata::locationCreated},
    {"locationShown", &umm::Metadata::locationShown},
    {"personShown", &umm::Metadata::personShown},
    {"productShown", &umm::Metadata::productShown},
    {"shownEvent", &umm::Metadata::shownEvent},
    {"registryEntry", &umm::Metadata::registryEntry},
    {"assetIdentifier", &umm::Metadata::assetIdentifier},
    {"aboutCvTerms", &umm::Metadata::aboutCvTerms},
    {"featuredOrganisation", &umm::Metadata::featuredOrganisation},
    {"supplier", &umm::Metadata::supplier},
};

}  // namespace

const std::vector<std::string_view>& accessor_names() {
  static const std::vector<std::string_view> names = [] {
    std::vector<std::string_view> v;
    for (const Binding& b : kBindings) v.push_back(b.name);
    return v;
  }();
  return names;
}

umm::Result<ResolvedProperty> resolve_property(std::string_view name, umm::MediaDomain) {
  if (name.find('.') != std::string_view::npos) {
    if (umm::registry().find(name)) return ResolvedProperty{std::string(name), std::string(name), nullptr};
  } else {
    for (const Binding& b : kBindings)
      if (b.name == name) return ResolvedProperty{std::string(name), "", b.getter};
  }
  return umm::Error{umm::ErrorCode::unknown_property, "unknown property '" + std::string(name) + "'", "", ""};
}

}  // namespace umm_cli
