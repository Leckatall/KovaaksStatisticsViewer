#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ksv::application {
    // The user's benchmarks by filename, ordered by how recently they mattered, most recent first.
    // Filenames that no longer exist stay in the list: it neither prunes nor caps itself.
    class IRecentBenchmarks {
    public:
        virtual ~IRecentBenchmarks() = default;

        [[nodiscard]] virtual const std::vector<std::string> &filenames() const = 0;
        // Moves the filename to the front, inserting it when absent.
        virtual void markViewed(const std::string &filename) = 0;
        // Inserts a new benchmark right behind the most recently viewed one (at the front of an
        // empty list) so creating one does not displace what the user was looking at. Does
        // nothing when the filename is already present.
        virtual void markCreated(const std::string &filename) = 0;
        // Replaces `from` where it stands, dropping any other occurrence of `to`. Does nothing
        // when `from` is absent.
        virtual void rename(const std::string &from, const std::string &to) = 0;
        // Fires only when the order or membership actually changed.
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}
