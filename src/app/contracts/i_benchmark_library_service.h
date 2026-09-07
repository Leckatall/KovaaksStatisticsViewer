#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "benchmark_library_snapshot.h"

namespace ksv::application {
    class IBenchmarkLibraryService {
    public:
        virtual ~IBenchmarkLibraryService() = default;
        [[nodiscard]] virtual std::optional<BenchmarkLibrarySnapshot> snapshot() const = 0;
        [[nodiscard]] virtual uint64_t revision() const = 0;
        [[nodiscard]] virtual bool lastRefreshFailed() const = 0;
        virtual void refresh() = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;
    };
}
