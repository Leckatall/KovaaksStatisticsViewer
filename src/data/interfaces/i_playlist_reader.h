#pragma once

#include <string>

#include "contracts/playlist_seed.h"

namespace ksv::application {
    // Reads one selected Kovaak's playlist export into an import seed, or a structured
    // failure. No benchmark draft is created or mutated here; the caller decides.
    class IPlaylistReader {
    public:
        virtual ~IPlaylistReader() = default;
        [[nodiscard]] virtual PlaylistReadResult read(const std::string &path) const = 0;
    };
}
