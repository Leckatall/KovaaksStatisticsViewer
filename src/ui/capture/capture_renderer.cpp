#include "capture/capture_renderer.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <type_traits>
#include <utility>
#include <variant>

#include "capture/capture_fonts.h"
#include "components/axis_painter.h"
#include "components/graph_scene_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kBorderWidth = 1.0;
        constexpr qreal kHatchSpacing = 6.0;
        constexpr qreal kHatchStripeWidth = 2.0;
        constexpr qreal kGlyphHaloWidth = 2.0;

        void paintRect(QPainter &painter, const CaptureRectItem &item) {
            painter.setPen(item.border.isValid() ? QPen(item.border, kBorderWidth) : QPen(Qt::NoPen));
            painter.setBrush(item.fill.isValid() ? QBrush(item.fill) : QBrush(Qt::NoBrush));
            painter.drawRoundedRect(item.rect, item.radius, item.radius);
        }

        void paintText(QPainter &painter, const CaptureLayout &layout, const CaptureTextItem &item) {
            painter.setFont(captureFont(layout.font, item.role));
            painter.setPen(item.color);
            const int flags = static_cast<int>(item.alignment) | Qt::TextDontClip;
            if (!item.vertical) {
                painter.drawText(item.rect, flags, item.text);
                return;
            }
            painter.save();
            painter.translate(item.rect.center());
            painter.rotate(270.0);
            const QSizeF rotated(item.rect.height(), item.rect.width());
            painter.drawText(QRectF(QPointF(-rotated.width() / 2.0, -rotated.height() / 2.0), rotated), flags,
                             item.text);
            painter.restore();
        }

        void paintHatch(QPainter &painter, const CaptureHatchItem &item) {
            painter.save();
            painter.setClipRect(item.rect, Qt::IntersectClip);
            painter.setPen(QPen(item.stripe, kHatchStripeWidth, Qt::SolidLine, Qt::FlatCap));
            const qreal height = item.rect.height();
            for (qreal x = item.rect.left() - height; x < item.rect.right(); x += kHatchSpacing) {
                painter.drawLine(QPointF(x, item.rect.bottom()), QPointF(x + height, item.rect.top()));
            }
            painter.restore();
        }

        QPainterPath glyphPath(const CaptureGlyphItem &item) {
            const QRectF &rect = item.rect;
            QPainterPath path;
            switch (item.kind) {
                case CaptureGlyphItem::Kind::Diamond:
                    path.moveTo(rect.center().x(), rect.top());
                    path.lineTo(rect.right(), rect.center().y());
                    path.lineTo(rect.center().x(), rect.bottom());
                    path.lineTo(rect.left(), rect.center().y());
                    path.closeSubpath();
                    break;
                case CaptureGlyphItem::Kind::Chevron:
                    path.moveTo(rect.topLeft());
                    path.lineTo(rect.right(), rect.center().y());
                    path.lineTo(rect.bottomLeft());
                    break;
                case CaptureGlyphItem::Kind::LineSample:
                    path.moveTo(rect.topLeft());
                    path.lineTo(rect.bottomRight());
                    break;
            }
            return path;
        }

        void paintGlyph(QPainter &painter, const CaptureGlyphItem &item) {
            const QPainterPath path = glyphPath(item);
            const auto strokePen = [](const QColor &color, const qreal width) {
                return QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            };
            if (item.halo.isValid()) painter.strokePath(path, strokePen(item.halo, item.lineWidth + kGlyphHaloWidth));
            if (item.kind == CaptureGlyphItem::Kind::Diamond) {
                painter.fillPath(path, item.color);
            } else {
                painter.strokePath(path, strokePen(item.color, item.lineWidth));
            }
        }

        void paintGraph(QPainter &painter, const CaptureLayout &layout, const CaptureGraphItem &item,
                        const CancellationCheck &cancelled) {
            if (!layout.scene) return;
            AxisPainter::Style style;
            style.font = captureFont(layout.font, CaptureTextRole::Tick);
            painter.save();
            paintGraphScene(painter, *layout.scene, item.rect, style, cancelled);
            painter.restore();
        }
    }

    std::optional<application::RasterImage> renderCapture(const CaptureLayout &layout, const QSize pixels,
                                                         const CancellationCheck &cancelled) {
        if (isCancelled(cancelled)) return std::nullopt;
        std::optional<application::RasterBuffer> buffer =
                application::RasterBuffer::allocate(pixels.width(), pixels.height());
        if (!buffer) return std::nullopt;

        {
            QImage image(reinterpret_cast<uchar *>(buffer->pixels()), pixels.width(), pixels.height(),
                         static_cast<qsizetype>(pixels.width()) * 4, QImage::Format_RGB32);
            image.fill(layout.background);

            QPainter painter(&image);
            if (!painter.isActive()) return std::nullopt;
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            painter.scale(pixels.width() / layout.size.width(), pixels.height() / layout.size.height());

            for (const CaptureItem &item : layout.items) {
                if (isCancelled(cancelled)) return std::nullopt;
                std::visit([&]<typename T>(const T &value) {
                    if constexpr (std::is_same_v<T, CaptureRectItem>) paintRect(painter, value);
                    else if constexpr (std::is_same_v<T, CaptureTextItem>) paintText(painter, layout, value);
                    else if constexpr (std::is_same_v<T, CaptureHatchItem>) paintHatch(painter, value);
                    else if constexpr (std::is_same_v<T, CaptureGlyphItem>) paintGlyph(painter, value);
                    else paintGraph(painter, layout, value, cancelled);
                }, item);
            }
        }

        if (isCancelled(cancelled)) return std::nullopt;
        return std::move(*buffer).freeze();
    }
}
