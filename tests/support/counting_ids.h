#ifndef KOVAAKSSTATSVIEWER_TESTS_COUNTING_IDS_H
#define KOVAAKSSTATSVIEWER_TESTS_COUNTING_IDS_H

#include <functional>
#include <memory>
#include <string>

namespace ksv::tests_support {
    // Deterministic monotonic id factory ("id-1", "id-2", ...) for the service's idFactory
    // seam, so tests can assert on element identity.
    inline std::function<std::string()> countingIds() {
        auto n = std::make_shared<int>(0);
        return [n] { return "id-" + std::to_string(++*n); };
    }
}

#endif
