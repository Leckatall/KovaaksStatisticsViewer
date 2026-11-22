#pragma once

#include <string>
#include <string_view>

namespace ksv::qt_data {
    // Unicode full case folding of UTF-8 text: the application's "ignoring letter case" for names.
    [[nodiscard]] std::string foldCase(std::string_view utf8);
}
