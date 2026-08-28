#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "series_config.h"

namespace ksv::application {
    // Canonical, Qt-free text form of an Expression, for copy/paste sharing between series and users.
    // Grammar: all-prefix `Name(args)`, PascalCase node names, FULLUPPER metric leaves, bare numeric
    // constant leaves; the scalar/selection parameter of RollingMean/AverageAcrossRuns is carried by a
    // required `window:` / `over:` label, expression inputs are positional. decode is case-insensitive
    // and whitespace-insensitive; encode is canonical (a decode of an encode re-encodes byte-identically).
    [[nodiscard]] std::string encodeExpressionDsl(const Expression &expression);

    [[nodiscard]] std::optional<Expression> decodeExpressionDsl(std::string_view text);
}
