#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_PLAYLIST_READER_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_PLAYLIST_READER_H

#include "data/interfaces/i_playlist_reader.h"

namespace ksv::tests_support {
    class FakePlaylistReader final : public application::IPlaylistReader {
    public:
        application::PlaylistReadResult nextResult{};
        mutable std::string lastPath;

        [[nodiscard]] application::PlaylistReadResult read(const std::string &path) const override {
            lastPath = path;
            return nextResult;
        }
    };
}

#endif
