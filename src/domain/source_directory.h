#ifndef KOVAAKSSTATSVIEWER_SOURCE_DIRECTORY_H
#define KOVAAKSSTATSVIEWER_SOURCE_DIRECTORY_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ksv::domain {
    struct DirectoryId {
        std::uint32_t value = 0;

        bool operator==(const DirectoryId &) const = default;
    };

    struct SourceDirectory {
        DirectoryId id;
        DirectoryId parent;
        std::string path;

        bool operator==(const SourceDirectory &) const = default;
    };

    struct SourceFileRef {
        DirectoryId directory;
        std::string filename;

        bool operator==(const SourceFileRef &) const = default;
    };

    class SourceRegistry {
    public:
        SourceRegistry() = default;
        explicit SourceRegistry(std::vector<SourceDirectory> entries);

        DirectoryId ensure(DirectoryId parent, std::string path);
        [[nodiscard]] std::optional<std::string> resolve(DirectoryId id) const;
        [[nodiscard]] std::optional<std::string> resolve(const SourceFileRef &source) const;
        [[nodiscard]] const std::vector<SourceDirectory> &entries() const;

    private:
        std::vector<SourceDirectory> m_entries;
        std::uint32_t m_next_id = 1;
    };
}

#endif //KOVAAKSSTATSVIEWER_SOURCE_DIRECTORY_H
