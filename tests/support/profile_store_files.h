#ifndef KOVAAKSSTATSVIEWER_TESTS_PROFILE_STORE_FILES_H
#define KOVAAKSSTATSVIEWER_TESTS_PROFILE_STORE_FILES_H

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ksv::tests_support {
    inline std::string readFile(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    // Quarantined stores sit beside the original as `<stem>_<reason>_<suffix><ext>`.
    inline std::vector<std::filesystem::path> quarantineFiles(const std::filesystem::path &profilePath,
                                                              const std::string &reason) {
        std::vector<std::filesystem::path> matches;
        const auto prefix = profilePath.stem().string() + "_" + reason + "_";
        for (const auto &entry: std::filesystem::directory_iterator(profilePath.parent_path())) {
            if (entry.path().extension() == profilePath.extension() &&
                entry.path().stem().string().starts_with(prefix)) {
                matches.push_back(entry.path());
            }
        }
        return matches;
    }
}

#endif
