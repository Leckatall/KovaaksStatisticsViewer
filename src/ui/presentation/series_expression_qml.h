#pragma once

#include <QVariantMap>

#include <optional>

#include "app/contracts/series_config.h"
#include "data/interfaces/i_series_config_store.h"

namespace ksv::presentation {
    [[nodiscard]] std::optional<application::Expression> parseExpression(const QVariantMap &map);
    [[nodiscard]] QVariantMap expressionMap(const application::Expression &expression);
    [[nodiscard]] QVariantMap mutationMap(const application::MutationResult &result);
    [[nodiscard]] QVariantMap invalidMutationMap();
}
