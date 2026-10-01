#include <umm/version.hpp>

#if !defined(UMM_VERSION_MAJOR)
#error "umm/version.hpp must define UMM_VERSION_MAJOR"
#endif

int main() { return umm::version().empty() ? 1 : 0; }
