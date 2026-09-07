#pragma once

#include <string>

#include "data/interfaces/i_benchmark_repository.h"

namespace ksv::qt_data {
    class BenchmarkRepository final : public application::IBenchmarkRepository {
    public:
        explicit BenchmarkRepository(std::string directoryPath);

        [[nodiscard]] application::BenchmarkScanResult scan() const override;
        application::BenchmarkWriteResult write(const domain::Benchmark &definition,
                                               const std::string &filename,
                                               const std::optional<std::string> &expectedDigest) override;
        application::BenchmarkDeleteResult remove(const std::string &filename,
                                                  const std::string &expectedDigest) override;
        [[nodiscard]] std::string managedDirectoryPath() const override;

    private:
        std::string m_directory;
    };
}
