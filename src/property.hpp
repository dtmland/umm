// Property-addressing seam shared by get/set/rm (concept §2.3).
#pragma once

#include <string>
#include <string_view>

#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"

namespace umm_cli {

struct ResolvedProperty {
  std::string name;         // what the user typed
  std::string property_id;  // libumm registry id
};

// Resolves a CLI name for a file of the given domain. Full ids (containing
// '.') are checked against libumm's registry. Convenience accessors (creator,
// gps, ...) are not yet enumerable through libumm's public headers at the
// pinned version, so they resolve to unknown_property here; sessions 06 and 08
// fill this in against libumm. The CLI never keeps its own accessor table.
umm::Result<ResolvedProperty> resolve_property(std::string_view name, umm::MediaDomain domain);

}  // namespace umm_cli
