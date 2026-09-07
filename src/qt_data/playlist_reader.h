#pragma once

#include "data/interfaces/i_playlist_reader.h"

namespace ksv::qt_data {
    class PlaylistReader final : public application::IPlaylistReader {
    public:
        [[nodiscard]] application::PlaylistReadResult read(const std::string &path) const override;
    };
}
