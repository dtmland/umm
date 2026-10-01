// Property-addressing seam shared by get/set/rm (concept §2.3).
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"

namespace umm_cli {

using AccessorGetter = std::optional<umm::PropertyValue> (umm::Metadata::*)() const;

struct ResolvedProperty {
  std::string name;               // what the user typed
  std::string property_id;        // registry id when addressed by full id; empty for accessors
  AccessorGetter getter{nullptr}; // libumm's public Metadata getter when addressed by accessor
  umm::Datatype datatype{umm::Datatype::text};
  bool photo_only{false};  // `rating`
  bool is_accessor() const { return getter != nullptr; }
};

// Resolves a CLI name:
//  - a full property id is checked against libumm's Registry;
//  - otherwise the name is bound to the matching public libumm Metadata
//    getter (creator, gps, ...). libumm does the cross-media (photo/video)
//    resolution, so the CLI holds no id mapping of its own.
// Unknown names return unknown_property. `domain` is for the setter path
// (session 08, Metadata::setMediaDomain); getters probe both domains.
umm::Result<ResolvedProperty> resolve_property(std::string_view name, umm::MediaDomain domain);

// Names of every bound accessor (for tests, help and completions).
const std::vector<std::string_view>& accessor_names();

// Typed setter / remove through libumm. Accessors use Metadata's public
// setters (domain already set on `metadata`); full ids use Metadata::set.
umm::Result<void> apply_set(umm::Metadata& metadata, const ResolvedProperty& property,
                            const umm::Value& value);
umm::Result<void> apply_remove(umm::Metadata& metadata, const ResolvedProperty& property);

}  // namespace umm_cli
