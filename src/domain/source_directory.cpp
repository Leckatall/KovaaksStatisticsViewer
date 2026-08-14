#include "source_directory.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace ksv::domain {
    SourceRegistry::SourceRegistry(std::vector<SourceDirectory> entries) : m_entries(std::move(entries)) {
        std::uint32_t max_id = 0;
        for (const auto &entry : m_entries) {
            max_id = std::max(max_id, entry.id.value);
        }
        m_next_id = max_id + 1;
    }

    DirectoryId SourceRegistry::ensure(const DirectoryId parent, std::string path) {
        const auto existing = std::ranges::find_if(m_entries, [&](const SourceDirectory &entry) {
            return entry.id.value != 0 && entry.parent == parent && entry.path == path;
        });
        if (existing != m_entries.end()) return existing->id;
        if (m_next_id == 0) throw std::overflow_error("source directory ids exhausted");

        const DirectoryId id{m_next_id++};
        m_entries.push_back({id, parent, std::move(path)});
        return id;
    }

    std::optional<std::string> SourceRegistry::resolve(DirectoryId id) const {
        if (id.value == 0) return std::nullopt;

        std::vector<std::string> segments;
        for (std::size_t depth = 0; depth < m_entries.size(); ++depth) {
            const auto entry = std::ranges::find_if(m_entries, [id](const SourceDirectory &candidate) {
                return candidate.id == id;
            });
            if (entry == m_entries.end()) return std::nullopt;
            segments.push_back(entry->path);
            if (entry->parent.value == 0) {
                std::filesystem::path resolved;
                for (auto segment = segments.rbegin(); segment != segments.rend(); ++segment) {
                    resolved /= *segment;
                }
                return resolved.generic_string();
            }
            id = entry->parent;
        }
        return std::nullopt;
    }

    std::optional<std::string> SourceRegistry::resolve(const SourceFileRef &source) const {
        const auto directory = resolve(source.directory);
        if (!directory) return std::nullopt;
        return (std::filesystem::path(*directory) / source.filename).generic_string();
    }

    const std::vector<SourceDirectory> &SourceRegistry::entries() const {
        return m_entries;
    }
}
