#pragma once

#include <string>

#include "data/interfaces/i_benchmark_store.h"

namespace ksv::qt_data {
    class BenchmarkStore final : public data::IBenchmarkStore {
    public:
        explicit BenchmarkStore(std::string directoryPath);

        [[nodiscard]] data::BenchmarkScanResult scan() const override;
        data::BenchmarkWriteResult write(const domain::Benchmark &definition,
                                         const std::string &filename,
                                         const std::optional<std::string> &expectedDigest) override;
        data::BenchmarkDeleteResult remove(const std::string &filename,
                                           const std::string &expectedDigest) override;
        [[nodiscard]] std::string managedDirectoryPath() const override;

    private:
        std::string m_directory;
    };
}
