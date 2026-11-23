#include "file_naming.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace ksv::data::files {
    namespace {
        constexpr std::size_t kComponentLimit = 255;

        // Byte length of the UTF-8 sequence starting with `lead`; malformed bytes count as one.
        std::size_t sequenceLength(const unsigned char lead) {
            if (lead >= 0xF0) return 4;
            if (lead >= 0xE0) return 3;
            if (lead >= 0xC0) return 2;
            return 1;
        }

        // Calls `visit(codePointBytes, utf16Units)` for each code point; stops when it returns false.
        template<typename Visit>
        void forEachCodePoint(const std::string_view text, Visit visit) {
            for (std::size_t i = 0; i < text.size();) {
                const auto length = std::min(sequenceLength(static_cast<unsigned char>(text[i])), text.size() - i);
                if (!visit(text.substr(i, length), length == 4 ? std::size_t{2} : std::size_t{1})) return;
                i += length;
            }
        }

        std::size_t lengthLimit(const std::string_view extension) {
            const auto reserved = utf16Length(extension);
            return reserved < kComponentLimit ? kComponentLimit - reserved : 0;
        }

        bool isInvalidByte(const unsigned char ch) {
            static constexpr std::string_view kInvalid = "<>:\"/\\|?*";
            return ch < 0x20 || kInvalid.find(static_cast<char>(ch)) != std::string_view::npos;
        }

        std::string truncateTo(const std::string_view text, const std::size_t limit) {
            std::string out;
            std::size_t units = 0;
            forEachCodePoint(text, [&](const std::string_view codePoint, const std::size_t width) {
                if (units + width > limit) return false;
                units += width;
                out += codePoint;
                return true;
            });
            return out;
        }

        void trimTrailingDotsAndSpaces(std::string &text) {
            while (!text.empty() && (text.back() == '.' || text.back() == ' ')) text.pop_back();
        }

        bool equalsIgnoringAsciiCase(const std::string_view a, const std::string_view b) {
            return a.size() == b.size() && std::ranges::equal(a, b, [](const char x, const char y) {
                return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            });
        }

        bool isReserved(const std::string_view name) {
            const auto base = name.substr(0, name.find('.'));
            static constexpr std::array<std::string_view, 4> kDevices{"CON", "PRN", "AUX", "NUL"};
            if (std::ranges::any_of(kDevices, [&](const auto device) { return equalsIgnoringAsciiCase(base, device); }))
                return true;
            if (base.size() != 4 || base[3] < '1' || base[3] > '9') return false;
            const auto prefix = base.substr(0, 3);
            return equalsIgnoringAsciiCase(prefix, "COM") || equalsIgnoringAsciiCase(prefix, "LPT");
        }
    }

    std::size_t utf16Length(const std::string_view utf8) {
        std::size_t units = 0;
        forEachCodePoint(utf8, [&](std::string_view, const std::size_t width) {
            units += width;
            return true;
        });
        return units;
    }

    std::string stemOf(const std::string_view filename, const std::string_view extension) {
        if (filename.size() >= extension.size() &&
            equalsIgnoringAsciiCase(filename.substr(filename.size() - extension.size()), extension))
            return std::string{filename.substr(0, filename.size() - extension.size())};
        return std::string{filename};
    }

    std::optional<FileNameProblem> checkName(const std::string_view name, const std::string_view extension) {
        if (std::ranges::all_of(name, [](const unsigned char ch) { return std::isspace(ch) != 0; }))
            return FileNameProblem::Blank;
        if (std::ranges::any_of(name, [](const char ch) { return isInvalidByte(static_cast<unsigned char>(ch)); }))
            return FileNameProblem::InvalidCharacter;
        if (utf16Length(name) > lengthLimit(extension)) return FileNameProblem::TooLong;
        if (name.back() == '.' || name.back() == ' ') return FileNameProblem::TrailingDotOrSpace;
        if (isReserved(name)) return FileNameProblem::Reserved;
        return std::nullopt;
    }

    std::string cleanName(const std::string_view raw, const std::string_view extension,
                          const std::string_view fallback) {
        std::string valid;
        std::ranges::copy_if(raw, std::back_inserter(valid),
                             [](const char ch) { return !isInvalidByte(static_cast<unsigned char>(ch)); });
        auto name = truncateTo(valid, lengthLimit(extension));
        trimTrailingDotsAndSpaces(name);
        if (name.empty() || isReserved(name)) return std::string{fallback};
        return name;
    }

    std::string uniqueName(const std::string_view name, const std::string_view extension,
                           const std::string_view fallback,
                           const std::function<bool(std::string_view)> &taken) {
        if (!taken(name)) return std::string{name};
        const auto limit = lengthLimit(extension);
        const auto shortened = [&](const std::string_view base, const std::size_t room) {
            auto cut = truncateTo(base, room);
            trimTrailingDotsAndSpaces(cut);
            return cut;
        };
        for (int n = 2;; ++n) {
            const auto suffix = " (" + std::to_string(n) + ")";
            const auto room = limit > suffix.size() ? limit - suffix.size() : 0;
            auto base = utf16Length(name) > room ? shortened(name, room) : std::string{name};
            if (base.empty()) base = shortened(fallback, room);
            auto candidate = base + suffix;
            if (!taken(candidate)) return candidate;
        }
    }
}
