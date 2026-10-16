#pragma once

#include <optional>
#include <string>

#include "app/contracts/graph_configuration.h"

namespace ksv::qt_data {
    inline constexpr int kGraphConfigurationSchemaVersion = 4;

    struct DecodedGraphConfiguration {
        application::GraphConfiguration configuration;
        // Some section failed to decode or validate and was replaced by its defaults, so the raw
        // document needs quarantining.
        bool hasRejectedSections = false;
        // The series section was among them; its replacement does not yet honour legacy settings.
        bool runSeriesRejected = false;
        // A version-4 document's own run-axis-label choice, including an explicit "none", which legacy
        // selections never override.
        bool runAxisLabelStored = false;
    };

    // Each section decodes independently; nullopt only when the text is not a JSON object with a
    // supported schemaVersion (1-4).
    [[nodiscard]] std::optional<DecodedGraphConfiguration> decodeGraphConfigurationDocument(const std::string &raw);

    [[nodiscard]] std::string encodeGraphConfigurationDocument(const application::GraphConfiguration &configuration);
}
