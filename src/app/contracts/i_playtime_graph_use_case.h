//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#include <functional>

#include "graph_presentation.h"
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

        [[nodiscard]] virtual GraphPresentation presentation() const = 0;

        // Fires for a committed style change only; the snapshot, and the profile behind it, are untouched.
        virtual void onPresentationChanged(std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
