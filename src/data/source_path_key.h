#ifndef KSV_SOURCE_PATH_KEY_H
#define KSV_SOURCE_PATH_KEY_H
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
namespace ksv::data {
    inline std::string sourcePathKey(std::string path) {
        std::ranges::replace(path, '\\', '/');
        path = std::filesystem::path(path).lexically_normal().generic_string();
#ifdef _WIN32
        std::ranges::transform(path, path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        while (path.size() > 1 && path.back() == '/') path.pop_back();
        return path;
    }
}
#endif
