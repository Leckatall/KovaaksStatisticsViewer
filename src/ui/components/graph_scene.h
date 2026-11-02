#ifndef KOVAAKSSTATSVIEWER_GRAPH_SCENE_H
#define KOVAAKSSTATSVIEWER_GRAPH_SCENE_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstdint>
#include <optional>

#include "series_painter.h"

namespace ksv::ui {
    // Every member of these structs is an owned value: no pointer, QObject or callable, so a scene
    // outlives the canvas and view model that built it and can be replayed off the GUI thread.

    // `labels` is parallel to `ticks`, already formatted.
    struct GraphSceneAxis {
        qreal min = 0.0;
        qreal max = 1.0;
        QList<qreal> ticks;
        QStringList labels;
        QString minLabel;
        QString maxLabel;

        // Maps value to [0,1] position on axis span; zero span maps to 0.5
        [[nodiscard]] qreal normalizedPosition(const qreal value) const {
            const qreal span = max - min;
            return span != 0.0 ? (value - min) / span : 0.5;
        }

        // Exact comparison holds because layoutLabels passes back the values it was given.
        [[nodiscard]] QString labelFor(const qreal tick) const {
            const qsizetype index = ticks.indexOf(tick);
            return index >= 0 && index < labels.size() ? labels.at(index) : QString();
        }
    };

    struct GraphSceneReference {
        qreal value = 0.0;
        QColor color;
    };

    struct GraphSceneSeries {
        int column = -1;
        QString id;
        QString name;
        // Already resolved against the foreground; a neutral series is never invalid here.
        QColor color;
        qreal lineWidth = 0.0;
        // Display space. Carries are kept apart: they are drawn through but never marked or sampled.
        QList<QPointF> observations;
        std::optional<QPointF> leadingCarry;
        std::optional<QPointF> trailingCarry;
        GraphSceneAxis xAxis;
        GraphSceneAxis yAxis;
        std::optional<uint64_t> yAxisId;
        QString yAxisName;
        QList<GraphSceneReference> references;
    };

    struct GraphSceneBand {
        qreal x = 0.0;
        QColor color;
    };

    struct GraphScene {
        // In user line-list order, not paint order.
        QList<GraphSceneSeries> series;
        GraphSceneAxis xAxis;
        std::optional<GraphSceneAxis> labelledYAxis;
        int labelledColumn = -1;
        QList<int> labelledColumns;
        QString labelledYAxisTitle;
        SeriesPainter::Connection connection = SeriesPainter::Connection::Smooth;
        bool pointMarkers = false;
        QColor background;
        // Live-only: capture scenes leave it empty.
        std::optional<GraphSceneBand> selectedBand;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_SCENE_H
