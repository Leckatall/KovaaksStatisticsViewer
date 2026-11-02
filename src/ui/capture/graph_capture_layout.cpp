#include "capture/graph_capture_layout.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>

#include "capture/capture_fonts.h"
#include "components/axis_painter.h"
#include "components/graph_scene_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kPageMargin = 16.0;
        constexpr qreal kSectionGap = 10.0;
        constexpr qreal kHeadingLineGap = 2.0;
        constexpr qreal kAxisTitleGap = 4.0;
        constexpr qreal kLegendRowGap = 4.0;
        constexpr qreal kLegendEntryGap = 18.0;
        constexpr qreal kLineSampleLength = 24.0;
        constexpr qreal kLineSampleGap = 6.0;
        constexpr int kMaximumHeightDoublings = 8;

        const Qt::Alignment kLeftAligned = Qt::AlignLeft | Qt::AlignVCenter;
        const Qt::Alignment kCentred = Qt::AlignHCenter | Qt::AlignVCenter;

        struct Scale {
            std::optional<uint64_t> yAxisId;
            QString title;
            QString minLabel;
            QString maxLabel;
            QList<GraphSceneSeries> series;
        };

        qreal lineHeight(const QFont &font) {
            return QFontMetricsF(font).height();
        }

        qreal measuredWidth(const QFont &font, const QString &text) {
            return std::ceil(QFontMetricsF(font).horizontalAdvance(text));
        }

        QStringList wrappedLines(const QString &text, const QFont &font, const qreal width) {
            // QTextLayout yields one blank line for empty text, which would reserve room for nothing.
            if (text.isEmpty()) return {};

            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            QTextLayout layout(text, font);
            layout.setTextOption(option);

            QStringList lines;
            layout.beginLayout();
            for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
                line.setLineWidth(width);
                lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
            }
            layout.endLayout();
            return lines;
        }

        std::vector<Scale> distinctScales(const GraphScene &scene) {
            std::vector<Scale> scales;
            for (const GraphSceneSeries &series : scene.series) {
                const auto shared = series.yAxisId
                                        ? std::ranges::find(scales, series.yAxisId, &Scale::yAxisId)
                                        : scales.end();
                if (shared != scales.end()) {
                    shared->series.append(series);
                    continue;
                }
                scales.push_back({series.yAxisId, series.yAxisName.isEmpty() ? series.name : series.yAxisName,
                                  series.yAxis.minLabel, series.yAxis.maxLabel, {series}});
            }
            return scales;
        }

        // Flows entries left to right, starting a row for each scale so that a scale's series read as its own.
        class LegendFlow {
        public:
            LegendFlow(const CaptureFontDescription &font, const QColor &color, const qreal left, const qreal width)
                : m_font(font), m_color(color), m_left(left), m_width(width), m_x(left),
                  m_rowHeight(std::max(lineHeight(captureFont(font, CaptureTextRole::Body)),
                                       lineHeight(captureFont(font, CaptureTextRole::BodyBold)))) {}

            void startScale(const Scale &scale) {
                if (!m_items.empty()) newRow();
                placeEntry(QStringLiteral("%1: %2 to %3").arg(scale.title, scale.minLabel, scale.maxLabel),
                           CaptureTextRole::BodyBold, nullptr);
            }

            void placeSeries(const GraphSceneSeries &series) {
                placeEntry(series.name, CaptureTextRole::Body, &series);
            }

            [[nodiscard]] qreal height() const {
                return m_items.empty() ? 0.0 : m_y + m_rowHeight;
            }

            // Rows are measured from zero, because the legend's top depends on the height it comes to.
            [[nodiscard]] std::vector<CaptureItem> itemsAt(const qreal top) const {
                std::vector<CaptureItem> items = m_items;
                for (CaptureItem &item : items) {
                    std::visit([top](auto &value) { value.rect.translate(0.0, top); }, item);
                }
                return items;
            }

        private:
            void newRow() {
                m_x = m_left;
                m_y += m_rowHeight + kLegendRowGap;
            }

            void placeEntry(const QString &text, const CaptureTextRole role, const GraphSceneSeries *series) {
                const QFont font = captureFont(m_font, role);
                const qreal indent = series ? kLineSampleLength + kLineSampleGap : 0.0;
                const QStringList lines = wrappedLines(text, font, m_width - indent);
                for (qsizetype i = 0; i < lines.size(); ++i) {
                    const qreal textWidth = measuredWidth(font, lines.at(i));
                    if (i > 0 || (m_x > m_left && m_x + indent + textWidth > m_left + m_width)) newRow();
                    if (i == 0 && series) appendSample(*series);
                    m_items.emplace_back(CaptureTextItem{QRectF(m_x + indent, m_y, textWidth, m_rowHeight), lines.at(i),
                                                         role, m_color, kLeftAligned, false});
                    m_x += indent + textWidth + kLegendEntryGap;
                }
            }

            void appendSample(const GraphSceneSeries &series) {
                CaptureGlyphItem sample;
                sample.kind = CaptureGlyphItem::Kind::LineSample;
                sample.rect = QRectF(m_x, m_y + m_rowHeight / 2.0, kLineSampleLength, 0.0);
                sample.color = series.color;
                sample.lineWidth = series.lineWidth;
                m_items.emplace_back(sample);
            }

            CaptureFontDescription m_font;
            QColor m_color;
            qreal m_left;
            qreal m_width;
            qreal m_x;
            qreal m_y = 0.0;
            qreal m_rowHeight;
            std::vector<CaptureItem> m_items;
        };

        CaptureRejection tooLittleRoom() {
            return {CaptureRejectionReason::TooLittleRoom, CaptureCorrection::LargerDimensions, std::nullopt};
        }
    }

    CaptureComposition provisionalCaptureComposition() {
        return {QSizeF(960.0, 600.0), QSizeF(240.0, 140.0)};
    }

    std::variant<CaptureLayout, CaptureRejection> layoutGraphCapture(const GraphCaptureSnapshot &snapshot,
                                                                     const QSizeF logical,
                                                                     const CaptureComposition &composition) {
        const qreal contentWidth = logical.width() - 2.0 * kPageMargin;
        if (contentWidth <= 0.0) return tooLittleRoom();

        const CaptureAppearance &appearance = snapshot.appearance;
        const GraphCaptureContext &context = snapshot.context;

        CaptureLayout layout;
        layout.size = logical;
        layout.background = snapshot.scene.background;
        layout.font = appearance.font;
        layout.scene = std::make_shared<const GraphScene>(snapshot.scene);

        qreal y = kPageMargin;
        const auto appendHeading = [&](const QString &text, const CaptureTextRole role, const QColor &color) {
            const QFont font = captureFont(appearance.font, role);
            const qreal height = lineHeight(font);
            for (const QString &line : wrappedLines(text, font, contentWidth)) {
                layout.items.emplace_back(CaptureTextItem{QRectF(kPageMargin, y, contentWidth, height), line, role,
                                                          color, kLeftAligned, false});
                y += height + kHeadingLineGap;
            }
        };
        appendHeading(context.title, CaptureTextRole::Title, appearance.text);
        appendHeading(context.graphType, CaptureTextRole::Context, appearance.placeholderText);
        appendHeading(context.rangeText, CaptureTextRole::Context, appearance.placeholderText);
        y += kSectionGap;

        LegendFlow legend(appearance.font, appearance.text, kPageMargin, contentWidth);
        for (const Scale &scale : distinctScales(snapshot.scene)) {
            legend.startScale(scale);
            for (const GraphSceneSeries &series : scale.series) legend.placeSeries(series);
        }

        const QFont axisTitleFont = captureFont(appearance.font, CaptureTextRole::Body);
        const qreal axisTitleHeight = lineHeight(axisTitleFont);
        const QStringList xTitleLines = wrappedLines(context.xTitle, axisTitleFont, contentWidth);
        const qreal xTitleBlock = static_cast<qreal>(xTitleLines.size()) * axisTitleHeight;

        const qreal graphHeight = logical.height() - kPageMargin - legend.height() - kSectionGap - xTitleBlock
                                  - kAxisTitleGap - y;
        if (graphHeight <= 0.0) return tooLittleRoom();

        qreal graphLeft = kPageMargin;
        for (const QString &line : wrappedLines(snapshot.scene.labelledYAxisTitle, axisTitleFont, graphHeight)) {
            layout.items.emplace_back(CaptureTextItem{QRectF(graphLeft, y, axisTitleHeight, graphHeight), line,
                                                      CaptureTextRole::Body, appearance.text, kCentred, true});
            graphLeft += axisTitleHeight;
        }
        if (graphLeft > kPageMargin) graphLeft += kAxisTitleGap;

        const QRectF graph(graphLeft, y, logical.width() - kPageMargin - graphLeft, graphHeight);
        AxisPainter::Style tickStyle;
        tickStyle.font = captureFont(appearance.font, CaptureTextRole::Tick);
        const QRectF plot = graph.width() > 0.0
                                ? graphPlotRect(graph, snapshot.scene.labelledYAxis, snapshot.scene.xAxis, tickStyle)
                                : QRectF();
        if (plot.width() < composition.minimumPlot.width() || plot.height() < composition.minimumPlot.height()) {
            return tooLittleRoom();
        }
        if (const auto &yAxis = snapshot.scene.labelledYAxis; snapshot.keepEveryYLabel && yAxis) {
            const auto placements = AxisPainter::layoutLabels(
                    plot, graph, AxisPainter::Orientation::Vertical, yAxis->min, yAxis->max, yAxis->ticks,
                    [&yAxis](const qreal tick) { return yAxis->labelFor(tick); }, tickStyle);
            if (placements.size() < yAxis->ticks.size()) return tooLittleRoom();
        }
        layout.items.emplace_back(CaptureGraphItem{graph});
        y += graphHeight + kAxisTitleGap;

        for (const QString &line : xTitleLines) {
            layout.items.emplace_back(CaptureTextItem{QRectF(kPageMargin, y, contentWidth, axisTitleHeight), line,
                                                      CaptureTextRole::Body, appearance.text, kCentred, false});
            y += axisTitleHeight;
        }
        y += kSectionGap;

        std::vector<CaptureItem> legendItems = legend.itemsAt(y);
        layout.items.insert(layout.items.end(), std::make_move_iterator(legendItems.begin()),
                            std::make_move_iterator(legendItems.end()));
        return layout;
    }

    // Searches with layoutGraphCapture itself: tier spacing has no closed form under layoutLabels' stride rule, and
    // the answer cannot drift from what validation accepts. Past the probe range, or for content that height cannot
    // help, it stays at the standard size and the layout's own rejection reports it.
    QSizeF graphReferenceSize(const GraphCaptureSnapshot &snapshot, const CaptureComposition &composition) {
        const QSizeF standard = composition.standardGraph;
        const auto fits = [&](const qreal height) {
            return std::holds_alternative<CaptureLayout>(
                    layoutGraphCapture(snapshot, QSizeF(standard.width(), height), composition));
        };
        if (fits(standard.height())) return standard;

        qreal tooShort = standard.height();
        qreal enough = tooShort * 2.0;
        for (int doubling = 0; !fits(enough); ++doubling) {
            if (doubling == kMaximumHeightDoublings) return standard;
            tooShort = enough;
            enough *= 2.0;
        }
        while (enough - tooShort > 1.0) {
            const qreal middle = (tooShort + enough) / 2.0;
            (fits(middle) ? enough : tooShort) = middle;
        }
        return {standard.width(), std::ceil(enough)};
    }

    qint64 captureSourcePoints(const GraphCaptureSnapshot &snapshot) {
        qint64 points = 0;
        for (const GraphSceneSeries &series : snapshot.scene.series) {
            points += series.observations.size();
            if (series.leadingCarry) ++points;
            if (series.trailingCarry) ++points;
        }
        return points;
    }

    std::optional<CaptureRejection> suggestGraphResolution(const GraphCaptureSnapshot &snapshot, const QSize pixels,
                                                           const int percent, const CaptureComposition &composition,
                                                           const CaptureLimits &limits) {
        const qint64 sourcePoints = captureSourcePoints(snapshot);
        if (sourcePoints > limits.maxSourcePoints) {
            return CaptureRejection{CaptureRejectionReason::SourceGeometry, CaptureCorrection::TooMuchData,
                                    std::nullopt};
        }

        for (const int mark : kCaptureResolutionMarks | std::views::reverse) {
            if (mark >= percent) continue;
            const QSizeF logical = QSizeF(pixels) / (mark / 100.0);
            if (!std::holds_alternative<CaptureLayout>(layoutGraphCapture(snapshot, logical, composition))) continue;
            if (validateCaptureOutput(CaptureSubject::Graph, logical, mark, sourcePoints, limits)) continue;
            return CaptureRejection{CaptureRejectionReason::TooLittleRoom, CaptureCorrection::LowerResolution, mark};
        }
        return tooLittleRoom();
    }
}
