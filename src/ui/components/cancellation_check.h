#pragma once

#include <functional>

namespace ksv::ui {
    // Polled between units of paint work, possibly off the GUI thread; an empty check never cancels.
    using CancellationCheck = std::function<bool()>;

    [[nodiscard]] inline bool isCancelled(const CancellationCheck &check) {
        return check && check();
    }
}
