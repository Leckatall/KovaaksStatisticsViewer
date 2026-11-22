#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <string>

#include "data/interfaces/i_benchmark_store.h"

namespace ksv::qt_data {
    class BenchmarkStore final : public data::IBenchmarkStore {
    public:
        // Every call that changes a file on disk, so a test can make one fail.
        struct FileOperations {
            std::function<bool(const QString &from, const QString &to)> rename;
            // Replaces the file whole or leaves it untouched.
            std::function<bool(const QString &path, const QByteArray &bytes)> writeAtomically;
        };
        [[nodiscard]] static FileOperations defaultFileOperations();

        explicit BenchmarkStore(std::string directoryPath);
        BenchmarkStore(std::string directoryPath, FileOperations operations);

        [[nodiscard]] data::BenchmarkScanResult scan() override;
        data::BenchmarkWriteResult write(const domain::BenchmarkDefinition &definition,
                                         const std::string &filename,
                                         const std::optional<std::string> &expectedDigest) override;
        data::BenchmarkRenameResult rename(const std::string &from, const std::string &to,
                                           const std::string &expectedDigest) override;
        data::BenchmarkDeleteResult remove(const std::string &filename,
                                           const std::string &expectedDigest) override;
        [[nodiscard]] std::string managedDirectoryPath() const override;

    private:
        std::string m_directory;
        FileOperations m_operations;
    };
}
