#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// Windows' rules for one path component, for any file the application names. Names are UTF-8.
namespace ksv::data::files {
    enum class FileNameProblem { Blank, InvalidCharacter, TooLong, TrailingDotOrSpace, Reserved };

    [[nodiscard]] std::size_t utf16Length(std::string_view utf8);

    // Removes a final `extension` matched ignoring ASCII case; other filenames are returned whole.
    [[nodiscard]] std::string stemOf(std::string_view filename, std::string_view extension);

    // The first rule `name` breaks, in enum order. The length limit leaves room for `extension`
    // within 255 UTF-16 units.
    [[nodiscard]] std::optional<FileNameProblem> checkName(std::string_view name, std::string_view extension);

    // Removes invalid characters, truncates to the length limit, trims trailing dots and spaces,
    // then substitutes `fallback` for an empty or reserved result.
    [[nodiscard]] std::string cleanName(std::string_view raw, std::string_view extension,
                                        std::string_view fallback);

    // `name`, or `name (N)` for the smallest N from 2 that `taken` refuses. `name` is shortened to
    // fit the suffix; `fallback` replaces it if shortening leaves nothing.
    [[nodiscard]] std::string uniqueName(std::string_view name, std::string_view extension,
                                         std::string_view fallback,
                                         const std::function<bool(std::string_view)> &taken);
}
