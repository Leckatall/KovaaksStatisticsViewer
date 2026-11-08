#pragma once

#include <QColor>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <QtGlobal>

#include <memory>
#include <variant>
#include <vector>

#include "capture/capture_font_description.h"
#include "capture/capture_fonts.h"
#include "components/graph_scene.h"

namespace ksv::ui {
    // Every coordinate here is logical: a layout is resolution-independent, and only the renderer knows pixels.

    struct CaptureRectItem {
        QRectF rect;
        QColor fill;
        QColor border;
        qreal radius = 0.0;

        bool operator==(const CaptureRectItem &) const = default;
    };

    // Exactly one measured line, so a resolution change cannot rewrap it.
    struct CaptureTextItem {
        QRectF rect;
        QString text;
        CaptureTextRole role = CaptureTextRole::Body;
        QColor color;
        Qt::Alignment alignment;
        bool vertical = false;

        bool operator==(const CaptureTextItem &) const = default;
    };

    struct CaptureHatchItem {
        QRectF rect;
        QColor stripe;

        bool operator==(const CaptureHatchItem &) const = default;
    };

    // A LineSample runs corner to corner of `rect`, so a horizontal sample has zero height. A Chevron points right
    // and spans `rect` exactly. An invalid `halo` paints none.
    struct CaptureGlyphItem {
        enum class Kind { Diamond, Chevron, LineSample };

        Kind kind = Kind::LineSample;
        QRectF rect;
        QColor color;
        QColor halo;
        qreal lineWidth = 0.0;

        bool operator==(const CaptureGlyphItem &) const = default;
    };

    struct CaptureGraphItem {
        QRectF rect;

        bool operator==(const CaptureGraphItem &) const = default;
    };

    using CaptureItem = std::variant<CaptureRectItem, CaptureTextItem, CaptureHatchItem, CaptureGlyphItem,
                                     CaptureGraphItem>;

    struct CaptureLayout {
        QSizeF size;
        QColor background;
        CaptureFontDescription font;
        std::vector<CaptureItem> items;
        std::shared_ptr<const GraphScene> scene;
    };

    [[nodiscard]] bool sameItems(const std::vector<CaptureItem> &a, const std::vector<CaptureItem> &b);
}
