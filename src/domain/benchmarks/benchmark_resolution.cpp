#include "benchmark_resolution.h"

#include <algorithm>

namespace ksv::domain {
    namespace {
        constexpr std::string_view kNoBreakSpace = "\xC2\xA0";

        bool isAsciiSpace(const char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
        }

        void trimFront(std::string_view &text) {
            while (!text.empty()) {
                if (isAsciiSpace(text.front())) text.remove_prefix(1);
                else if (text.starts_with(kNoBreakSpace)) text.remove_prefix(kNoBreakSpace.size());
                else return;
            }
        }

        void trimBack(std::string_view &text) {
            while (!text.empty()) {
                if (isAsciiSpace(text.back())) text.remove_suffix(1);
                else if (text.ends_with(kNoBreakSpace)) text.remove_suffix(kNoBreakSpace.size());
                else return;
            }
        }
    }

    std::string scenarioNameKey(std::string_view name) {
        trimFront(name);
        trimBack(name);
        std::string key(name);
        std::ranges::transform(key, key.begin(), [](const char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        });
        return key;
    }
}
