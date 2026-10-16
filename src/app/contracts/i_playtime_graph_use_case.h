//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#include <functional>

#include "playtime_view.h"

namespace ksv::application {
    struct PlaytimeGraphSnapshot {
        PlaytimeView view;
        // Only the samples the committed range shows; each keeps its full-history value.
        PlaytimeSeries samples;
    };

    class IPlaytimeGraphUseCase {
    public:
        virtual ~IPlaytimeGraphUseCase() = default;

        [[nodiscard]] virtual PlaytimeGraphSnapshot snapshot() = 0;

        // Fired when a new profile, a committed change to this graph's preferences, or a day change
        // that moves a today-anchored range makes the snapshot differ.
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
