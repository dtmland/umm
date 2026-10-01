#include "property.hpp"

#include <variant>

namespace umm_cli {

namespace {

constexpr std::string_view kGpsId = "exif.gps.position";
constexpr std::string_view kRatingId = "iptc.photo.imageRating";

umm::Error invalid_value(std::string_view name) {
  return umm::Error{umm::ErrorCode::invalid_value,
                    "value does not match datatype for " + std::string(name), "", ""};
}

umm::Error unknown_property(std::string_view name) {
  return umm::Error{umm::ErrorCode::unknown_property, "unknown property '" + std::string(name) + "'",
                    "", ""};
}

// One entry per public `std::optional<PropertyValue> NAME() const` getter in
// libumm's include/umm/metadata.hpp (v0.1.0). Datatype is the photo-native
// setter argument (libumm transposes for video). Ids stay in libumm.
struct Binding {
  std::string_view name;
  AccessorGetter getter;
  umm::Datatype datatype;
  bool photo_only{false};
};

const Binding kBindings[] = {
    {"creator", &umm::Metadata::creator, umm::Datatype::text_list},
    {"description", &umm::Metadata::description, umm::Datatype::lang_alt},
    {"headline", &umm::Metadata::headline, umm::Datatype::text},
    {"dateCreated", &umm::Metadata::dateCreated, umm::Datatype::date_time},
    {"copyrightNotice", &umm::Metadata::copyrightNotice, umm::Datatype::lang_alt},
    {"creditLine", &umm::Metadata::creditLine, umm::Datatype::text},
    {"keywords", &umm::Metadata::keywords, umm::Datatype::text_list},
    {"rating", &umm::Metadata::rating, umm::Datatype::real, true},
    {"title", &umm::Metadata::title, umm::Datatype::lang_alt},
    {"altTextAccessibility", &umm::Metadata::altTextAccessibility, umm::Datatype::lang_alt},
    {"extendedDescriptionAccessibility", &umm::Metadata::extendedDescriptionAccessibility,
     umm::Datatype::lang_alt},
    {"rightsUsageTerms", &umm::Metadata::rightsUsageTerms, umm::Datatype::lang_alt},
    {"sourceSupplyChain", &umm::Metadata::sourceSupplyChain, umm::Datatype::text},
    {"dataMining", &umm::Metadata::dataMining, umm::Datatype::text},
    {"contributor", &umm::Metadata::contributor, umm::Datatype::structure_list},
    {"genre", &umm::Metadata::genre, umm::Datatype::structure_list},
    {"embeddedEncodedRightsExpression", &umm::Metadata::embeddedEncodedRightsExpression,
     umm::Datatype::structure_list},
    {"linkedEncodedRightsExpression", &umm::Metadata::linkedEncodedRightsExpression,
     umm::Datatype::structure_list},
    {"aiPromptInformation", &umm::Metadata::aiPromptInformation, umm::Datatype::text},
    {"aiPromptWriterName", &umm::Metadata::aiPromptWriterName, umm::Datatype::text},
    {"aiSystemUsed", &umm::Metadata::aiSystemUsed, umm::Datatype::text},
    {"aiSystemVersionUsed", &umm::Metadata::aiSystemVersionUsed, umm::Datatype::text},
    {"otherConstraints", &umm::Metadata::otherConstraints, umm::Datatype::lang_alt},
    {"digitalSourceType", &umm::Metadata::digitalSourceType, umm::Datatype::text},
    {"modelReleaseStatus", &umm::Metadata::modelReleaseStatus, umm::Datatype::text},
    {"propertyReleaseStatus", &umm::Metadata::propertyReleaseStatus, umm::Datatype::text},
    {"copyrightOwner", &umm::Metadata::copyrightOwner, umm::Datatype::structure_list},
    {"licensor", &umm::Metadata::licensor, umm::Datatype::structure_list},
    {"gps", &umm::Metadata::gps, umm::Datatype::gps_coordinate},
    {"locationCreated", &umm::Metadata::locationCreated, umm::Datatype::structure_list},
    {"locationShown", &umm::Metadata::locationShown, umm::Datatype::structure_list},
    {"personShown", &umm::Metadata::personShown, umm::Datatype::structure_list},
    {"productShown", &umm::Metadata::productShown, umm::Datatype::structure_list},
    {"shownEvent", &umm::Metadata::shownEvent, umm::Datatype::structure_list},
    {"registryEntry", &umm::Metadata::registryEntry, umm::Datatype::structure_list},
    {"assetIdentifier", &umm::Metadata::assetIdentifier, umm::Datatype::text},
    {"aboutCvTerms", &umm::Metadata::aboutCvTerms, umm::Datatype::structure_list},
    {"featuredOrganisation", &umm::Metadata::featuredOrganisation, umm::Datatype::text_list},
    {"supplier", &umm::Metadata::supplier, umm::Datatype::structure_list},
};

const Binding* find_binding(std::string_view name) {
  for (const Binding& b : kBindings)
    if (b.name == name) return &b;
  return nullptr;
}

umm::Datatype datatype_for_id(std::string_view id) {
  if (id == kGpsId) return umm::Datatype::gps_coordinate;
  if (auto def = umm::registry().find(id)) return def->datatype;
  return umm::Datatype::text;
}

bool known_id(std::string_view id) { return id == kGpsId || umm::registry().find(id).has_value(); }

template <typename T>
umm::Result<void> set_alt(umm::Metadata& meta, const umm::Value& value,
                          umm::Result<void> (umm::Metadata::*fn)(T), std::string_view name) {
  if (const auto* v = std::get_if<T>(&value.data)) return (meta.*fn)(*v);
  return invalid_value(name);
}

}  // namespace

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
    if (known_id(name))
      return ResolvedProperty{std::string(name), std::string(name), nullptr, datatype_for_id(name), false};
  } else if (const Binding* b = find_binding(name)) {
    return ResolvedProperty{std::string(name), "", b->getter, b->datatype, b->photo_only};
  }
  return unknown_property(name);
}

umm::Result<void> apply_set(umm::Metadata& metadata, const ResolvedProperty& property,
                            const umm::Value& value) {
  if (property.photo_only && metadata.mediaDomain() == umm::MediaDomain::video)
    return umm::Error{umm::ErrorCode::invalid_value, "rating is photo-only", "", ""};
  if (!property.is_accessor()) return metadata.set(property.property_id, value);

  const std::string_view name = property.name;
  if (name == "creator") return set_alt(metadata, value, &umm::Metadata::setCreator, name);
  if (name == "description") return set_alt(metadata, value, &umm::Metadata::setDescription, name);
  if (name == "headline") return set_alt(metadata, value, &umm::Metadata::setHeadline, name);
  if (name == "dateCreated") return set_alt(metadata, value, &umm::Metadata::setDateCreated, name);
  if (name == "copyrightNotice") return set_alt(metadata, value, &umm::Metadata::setCopyrightNotice, name);
  if (name == "creditLine") return set_alt(metadata, value, &umm::Metadata::setCreditLine, name);
  if (name == "keywords") return set_alt(metadata, value, &umm::Metadata::setKeywords, name);
  if (name == "rating") return set_alt(metadata, value, &umm::Metadata::setRating, name);
  if (name == "title") return set_alt(metadata, value, &umm::Metadata::setTitle, name);
  if (name == "altTextAccessibility")
    return set_alt(metadata, value, &umm::Metadata::setAltTextAccessibility, name);
  if (name == "extendedDescriptionAccessibility")
    return set_alt(metadata, value, &umm::Metadata::setExtendedDescriptionAccessibility, name);
  if (name == "rightsUsageTerms") return set_alt(metadata, value, &umm::Metadata::setRightsUsageTerms, name);
  if (name == "sourceSupplyChain")
    return set_alt(metadata, value, &umm::Metadata::setSourceSupplyChain, name);
  if (name == "dataMining") return set_alt(metadata, value, &umm::Metadata::setDataMining, name);
  if (name == "contributor") return set_alt(metadata, value, &umm::Metadata::setContributor, name);
  if (name == "genre") return set_alt(metadata, value, &umm::Metadata::setGenre, name);
  if (name == "embeddedEncodedRightsExpression")
    return set_alt(metadata, value, &umm::Metadata::setEmbeddedEncodedRightsExpression, name);
  if (name == "linkedEncodedRightsExpression")
    return set_alt(metadata, value, &umm::Metadata::setLinkedEncodedRightsExpression, name);
  if (name == "aiPromptInformation")
    return set_alt(metadata, value, &umm::Metadata::setAiPromptInformation, name);
  if (name == "aiPromptWriterName")
    return set_alt(metadata, value, &umm::Metadata::setAiPromptWriterName, name);
  if (name == "aiSystemUsed") return set_alt(metadata, value, &umm::Metadata::setAiSystemUsed, name);
  if (name == "aiSystemVersionUsed")
    return set_alt(metadata, value, &umm::Metadata::setAiSystemVersionUsed, name);
  if (name == "otherConstraints") return set_alt(metadata, value, &umm::Metadata::setOtherConstraints, name);
  if (name == "digitalSourceType")
    return set_alt(metadata, value, &umm::Metadata::setDigitalSourceType, name);
  if (name == "modelReleaseStatus")
    return set_alt(metadata, value, &umm::Metadata::setModelReleaseStatus, name);
  if (name == "propertyReleaseStatus")
    return set_alt(metadata, value, &umm::Metadata::setPropertyReleaseStatus, name);
  if (name == "copyrightOwner") return set_alt(metadata, value, &umm::Metadata::setCopyrightOwner, name);
  if (name == "licensor") return set_alt(metadata, value, &umm::Metadata::setLicensor, name);
  if (name == "gps") return set_alt(metadata, value, &umm::Metadata::setGps, name);
  if (name == "locationCreated") return set_alt(metadata, value, &umm::Metadata::setLocationCreated, name);
  if (name == "locationShown") return set_alt(metadata, value, &umm::Metadata::setLocationShown, name);
  if (name == "personShown") return set_alt(metadata, value, &umm::Metadata::setPersonShown, name);
  if (name == "productShown") return set_alt(metadata, value, &umm::Metadata::setProductShown, name);
  if (name == "shownEvent") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    if (!list || list->empty()) return invalid_value(name);
    const umm::Structure& fields = list->front();
    umm::LangAlt event_name;
    std::vector<std::string> identifiers;
    if (auto it = fields.find("name"); it != fields.end()) {
      if (const auto* alt = std::get_if<umm::LangAlt>(&it->second.data))
        event_name = *alt;
      else if (const auto* text = std::get_if<std::string>(&it->second.data))
        event_name.emplace("x-default", *text);
    }
    if (auto it = fields.find("identifiers"); it != fields.end()) {
      if (const auto* ids = std::get_if<std::vector<std::string>>(&it->second.data))
        identifiers = *ids;
      else if (const auto* id = std::get_if<std::string>(&it->second.data))
        identifiers.push_back(*id);
    }
    return metadata.setShownEvent(std::move(event_name), std::move(identifiers));
  }
  if (name == "registryEntry") return set_alt(metadata, value, &umm::Metadata::setRegistryEntry, name);
  if (name == "assetIdentifier") return set_alt(metadata, value, &umm::Metadata::setAssetIdentifier, name);
  if (name == "aboutCvTerms") return set_alt(metadata, value, &umm::Metadata::setAboutCvTerms, name);
  if (name == "featuredOrganisation")
    return set_alt(metadata, value, &umm::Metadata::setFeaturedOrganisation, name);
  if (name == "supplier") return set_alt(metadata, value, &umm::Metadata::setSupplier, name);
  return unknown_property(name);
}

umm::Result<void> apply_remove(umm::Metadata& metadata, const ResolvedProperty& property) {
  if (!property.is_accessor()) return metadata.remove(property.property_id);
  if (property.photo_only && metadata.mediaDomain() == umm::MediaDomain::video)
    return umm::Error{umm::ErrorCode::invalid_value, "rating is photo-only", "", ""};

  std::optional<umm::PropertyValue> current = (metadata.*(property.getter))();
  if (!current) return {};
  std::vector<std::string> ids = metadata.propertyIds();
  bool removed = false;
  for (const std::string& id : ids) {
    std::optional<umm::PropertyValue> pv = metadata.get(id);
    if (pv && pv->value == current->value) {
      umm::Result<void> r = metadata.remove(id);
      if (!r.ok()) return r;
      removed = true;
    }
  }
  if (removed) return {};
  if (property.name == "rating") return metadata.remove(std::string(kRatingId));
  if (property.name == "gps") return metadata.remove(std::string(kGpsId));
  return {};
}

}  // namespace umm_cli
