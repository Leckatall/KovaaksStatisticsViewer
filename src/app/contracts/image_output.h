#pragma once

#include <cstdint>
#include <string>

namespace ksv::application {
    enum class ImageOutputKind { Copy, SavePng };

    enum class ImageOutputStatus {
        Succeeded,
        ClipboardNotConfirmed,
        DestinationUnavailable,
        EncodeFailed,
        CommitFailed,
        Unexpected,
    };

    // A rejected call has accepted == false and operationId == 0, and no result follows it.
    struct ImageOutputAdmission {
        bool accepted = false;
        std::uint64_t operationId = 0;
    };

    struct ImageOutputResult {
        std::uint64_t operationId = 0;
        ImageOutputKind kind = ImageOutputKind::Copy;
        std::string targetLabel;
        ImageOutputStatus status = ImageOutputStatus::Succeeded;
    };
}
