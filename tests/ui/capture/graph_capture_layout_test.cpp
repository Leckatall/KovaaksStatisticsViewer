#include <gtest/gtest.h>

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

#include "capture/capture_fonts.h"
#include "capture/capture_layout.h"
#include "capture/capture_limits.h"
#include "capture/capture_renderer.h"
#include "capture/capture_sizing.h"
#include "capture/capture_snapshot.h"
#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "capture_test_support.h"
#include "components/axis_painter.h"
#include "components/graph_canvas.h"
#include "components/graph_scene_painter.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

namespace ksv::ui {
    namespace {
        const QSizeF kLogical(960.0, 600.0);

        CaptureComposition testComposition() {
            CaptureComposition composition;
            composition.standardGraph = QSizeF(880.0, 540.0);
            composition.minimumPlot = QSizeF(220.0, 120.0);
            return composition;
        }

        CaptureFontDescription applicationDescription() {
            const QFont app = QGuiApplication::font();
            CaptureFontDescription description;
            description.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
            description.pixelSize = 14;
            return description;
        }

        GraphSceneAxis axis(const qreal min, const qreal max, const QString &minLabel, const QString &maxLabel) {
            GraphSceneAxis result;
            result.min = min;
            result.max = max;
            result.ticks = {min, max};
            result.labels = {minLabel, maxLabel};
            result.minLabel = minLabel;
            result.maxLabel = maxLabel;
            return result;
        }

        GraphSceneSeries series(const int column, const QString &name, const QColor &color, const qreal lineWidth,
                                const GraphSceneAxis &yAxis) {
            GraphSceneSeries result;
            result.column = column;
            result.id = QStringLiteral("series-%1").arg(column);
            result.name = name;
            result.color = color;
            result.lineWidth = lineWidth;
            result.observations = {QPointF(0.0, yAxis.min), QPointF(5.0, yAxis.max), QPointF(10.0, yAxis.min)};
            result.yAxis = yAxis;
            return result;
        }

        GraphSceneSeries scaledSeries(const int column, const QString &name, const QColor &color,
                                      const GraphSceneAxis &yAxis, const uint64_t yAxisId, const QString &yAxisName) {
            GraphSceneSeries result = series(column, name, color, 2.0, yAxis);
            result.yAxisId = yAxisId;
            result.yAxisName = yAxisName;
            return result;
        }

        GraphCaptureSnapshot snapshotOf(const QList<GraphSceneSeries> &seriesList) {
            GraphCaptureSnapshot snapshot;
            snapshot.target = CaptureTarget::Run;
            snapshot.context.title = QStringLiteral("Pasu Track Invincible");
            snapshot.context.graphType = QStringLiteral("Run graph");
            snapshot.context.rangeText = QStringLiteral("Elapsed 0:00 to 1:00");
            snapshot.context.xTitle = QStringLiteral("Elapsed seconds");
            snapshot.appearance.window = QColor(16, 24, 32);
            snapshot.appearance.text = QColor(240, 240, 240);
            snapshot.appearance.placeholderText = QColor(160, 160, 160);
            snapshot.appearance.font = applicationDescription();

            GraphSceneAxis xAxis = axis(0.0, 10.0, QStringLiteral("0"), QStringLiteral("10"));
            for (GraphSceneSeries entry : seriesList) {
                entry.xAxis = xAxis;
                snapshot.scene.series.append(entry);
            }
            snapshot.scene.xAxis = xAxis;
            snapshot.scene.labelledYAxis = seriesList.first().yAxis;
            snapshot.scene.labelledColumn = seriesList.first().column;
            snapshot.scene.labelledColumns = {seriesList.first().column};
            snapshot.scene.labelledYAxisTitle = QStringLiteral("Damage done");
            snapshot.scene.connection = SeriesPainter::Connection::Straight;
            snapshot.scene.background = snapshot.appearance.window;
            return snapshot;
        }

        GraphCaptureSnapshot ordinarySnapshot() {
            const GraphSceneAxis yAxis = axis(0.0, 100.0, QStringLiteral("0"), QStringLiteral("100"));
            return snapshotOf({series(1, QStringLiteral("Damage"), Qt::red, 3.0, yAxis)});
        }

        CaptureLayout layoutOf(const GraphCaptureSnapshot &snapshot, const QSizeF &logical,
                               const CaptureComposition &composition = testComposition()) {
            std::variant<CaptureLayout, CaptureRejection> result = layoutGraphCapture(snapshot, logical, composition);
            if (auto *layout = std::get_if<CaptureLayout>(&result)) return *layout;
            ADD_FAILURE() << "layoutGraphCapture rejected the snapshot";
            return {};
        }

        std::vector<CaptureGraphItem> graphItems(const CaptureLayout &layout) {
            std::vector<CaptureGraphItem> graphs;
            for (const CaptureItem &item : layout.items) {
                if (const auto *graph = std::get_if<CaptureGraphItem>(&item)) graphs.push_back(*graph);
            }
            return graphs;
        }

        std::vector<CaptureGlyphItem> glyphItems(const CaptureLayout &layout) {
            std::vector<CaptureGlyphItem> glyphs;
            for (const CaptureItem &item : layout.items) {
                if (const auto *glyph = std::get_if<CaptureGlyphItem>(&item)) glyphs.push_back(*glyph);
            }
            return glyphs;
        }

        QRectF itemRect(const CaptureItem &item) {
            return std::visit([](const auto &value) { return value.rect; }, item);
        }

        // Spelled out because QRectF::contains is false for an empty rect, and a horizontal line sample has no height.
        bool liesWithin(const QRectF &rect, const QRectF &bounds) {
            return rect.left() >= bounds.left() && rect.top() >= bounds.top() && rect.right() <= bounds.right()
                   && rect.bottom() <= bounds.bottom();
        }

        QRectF plotRect(const CaptureLayout &layout, const GraphCaptureSnapshot &snapshot) {
            const std::vector<CaptureGraphItem> graphs = graphItems(layout);
            if (graphs.size() != 1) {
                ADD_FAILURE() << "expected exactly one graph item, found " << graphs.size();
                return {};
            }
            AxisPainter::Style style;
            style.font = captureFont(layout.font, CaptureTextRole::Tick);
            return graphPlotRect(graphs.front().rect, snapshot.scene.labelledYAxis, snapshot.scene.xAxis, style);
        }

        int indexOfText(const std::vector<CaptureTextItem> &texts, const QString &text) {
            const auto found = std::ranges::find_if(texts, [&](const CaptureTextItem &item) {
                return item.text == text;
            });
            return found == texts.end() ? -1 : static_cast<int>(std::distance(texts.begin(), found));
        }

        qsizetype textsContaining(const std::vector<CaptureTextItem> &texts, const QString &needle) {
            return std::ranges::count_if(texts, [&](const CaptureTextItem &item) {
                return item.text.contains(needle);
            });
        }

        int indexOfScaleEntry(const std::vector<CaptureTextItem> &texts, const QString &title,
                              const GraphSceneAxis &yAxis) {
            const auto found = std::ranges::find_if(texts, [&](const CaptureTextItem &item) {
                return item.text.contains(title) && item.text.contains(yAxis.minLabel)
                       && item.text.contains(yAxis.maxLabel);
            });
            return found == texts.end() ? -1 : static_cast<int>(std::distance(texts.begin(), found));
        }
    }

    TEST(GraphCaptureLayout, HoldsIdentityAxisTitlesAndPlot) {
        const GraphCaptureSnapshot snapshot = ordinarySnapshot();

        const CaptureLayout layout = layoutOf(snapshot, kLogical);

        const QString joined = joinedCaptureText(layout.items);
        ASSERT_TRUE(joined.contains(snapshot.context.title));
        EXPECT_TRUE(joined.contains(snapshot.context.graphType));
        EXPECT_TRUE(joined.contains(snapshot.context.rangeText));
        EXPECT_TRUE(joined.contains(snapshot.context.xTitle));

        const std::vector<CaptureTextItem> texts = captureTextItems(layout.items);
        const int yTitle = indexOfText(texts, snapshot.scene.labelledYAxisTitle);
        ASSERT_GE(yTitle, 0);
        EXPECT_TRUE(texts[yTitle].vertical);

        const std::vector<CaptureGraphItem> graphs = graphItems(layout);
        ASSERT_EQ(graphs.size(), 1u);
        const QRectF canvas(QPointF(0.0, 0.0), layout.size);
        EXPECT_EQ(layout.size, kLogical);
        EXPECT_TRUE(liesWithin(graphs.front().rect, canvas));
        ASSERT_NE(layout.scene, nullptr);

        for (const CaptureTextItem &text : texts) {
            EXPECT_FALSE(text.rect.intersects(graphs.front().rect)) << text.text.toStdString();
        }
        for (const CaptureItem &item : layout.items) {
            EXPECT_TRUE(liesWithin(itemRect(item), canvas));
        }
    }

    TEST(GraphCaptureLayout, LegendListsEachSeriesInLineOrderWithItsStyle) {
        const GraphSceneAxis yAxis = axis(0.0, 100.0, QStringLiteral("0"), QStringLiteral("100"));
        const GraphCaptureSnapshot snapshot = snapshotOf({
            series(1, QStringLiteral("Zulu line"), Qt::red, 1.5, yAxis),
            series(2, QStringLiteral("Alpha line"), Qt::green, 3.0, yAxis),
            series(3, QStringLiteral("Mike line"), Qt::blue, 4.5, yAxis),
        });

        const CaptureLayout layout = layoutOf(snapshot, kLogical);

        const std::vector<CaptureTextItem> texts = captureTextItems(layout.items);
        const std::vector<CaptureGlyphItem> glyphs = glyphItems(layout);
        ASSERT_EQ(glyphs.size(), static_cast<std::size_t>(snapshot.scene.series.size()));

        int previous = -1;
        for (qsizetype i = 0; i < snapshot.scene.series.size(); ++i) {
            const GraphSceneSeries &line = snapshot.scene.series.at(i);
            const int name = indexOfText(texts, line.name);
            ASSERT_GT(name, previous) << line.name.toStdString();
            previous = name;

            const CaptureGlyphItem &glyph = glyphs[static_cast<std::size_t>(i)];
            EXPECT_EQ(glyph.kind, CaptureGlyphItem::Kind::LineSample);
            EXPECT_EQ(glyph.color, line.color);
            EXPECT_EQ(glyph.lineWidth, line.lineWidth);
            EXPECT_LE(glyph.rect.right(), texts[name].rect.left());
            EXPECT_GE(glyph.rect.center().y(), texts[name].rect.top());
            EXPECT_LE(glyph.rect.center().y(), texts[name].rect.bottom());
        }

        for (const CaptureItem &item : layout.items) {
            EXPECT_FALSE(std::holds_alternative<CaptureRectItem>(item));
            EXPECT_FALSE(std::holds_alternative<CaptureHatchItem>(item));
        }
    }

    TEST(GraphCaptureLayout, EachDistinctScaleAppearsOnceWithBoundsAndUnits) {
        const GraphSceneAxis scoreAxis = axis(1240.0, 9870.0, QStringLiteral("1,240"), QStringLiteral("9,870"));
        const GraphSceneAxis accuracyAxis = axis(61.5, 98.2, QStringLiteral("61.5%"), QStringLiteral("98.2%"));
        const GraphSceneAxis killsAxis = axis(3.5, 12.0, QStringLiteral("3.5/s"), QStringLiteral("12.0/s"));
        const QString scoreName = QStringLiteral("Score");
        const QString averageName = QStringLiteral("Score average");
        const QString accuracyName = QStringLiteral("Accuracy");
        const QString killsName = QStringLiteral("Kills");

        // Accuracy sits between the two Score series, so grouping has to pull the average back under its scale.
        const GraphCaptureSnapshot snapshot = snapshotOf({
            scaledSeries(1, scoreName, Qt::red, scoreAxis, 7, scoreName),
            scaledSeries(2, accuracyName, Qt::green, accuracyAxis, 8, accuracyName),
            scaledSeries(3, averageName, Qt::yellow, scoreAxis, 7, scoreName),
            series(4, killsName, Qt::blue, 2.0, killsAxis),
        });

        const CaptureLayout layout = layoutOf(snapshot, kLogical);

        const std::vector<CaptureTextItem> texts = captureTextItems(layout.items);
        const int scoreEntry = indexOfScaleEntry(texts, scoreName, scoreAxis);
        const int accuracyEntry = indexOfScaleEntry(texts, accuracyName, accuracyAxis);
        const int killsEntry = indexOfScaleEntry(texts, killsName, killsAxis);
        ASSERT_GE(scoreEntry, 0);
        ASSERT_GE(accuracyEntry, 0);
        ASSERT_GE(killsEntry, 0);

        for (const GraphSceneAxis &scale : {scoreAxis, accuracyAxis, killsAxis}) {
            EXPECT_EQ(textsContaining(texts, scale.minLabel), 1) << scale.minLabel.toStdString();
            EXPECT_EQ(textsContaining(texts, scale.maxLabel), 1) << scale.maxLabel.toStdString();
        }

        const auto exactly = [&](const QString &name) {
            return std::ranges::count_if(texts, [&](const CaptureTextItem &item) { return item.text == name; });
        };
        EXPECT_EQ(exactly(scoreName), 1);
        EXPECT_EQ(exactly(averageName), 1);

        EXPECT_GT(indexOfText(texts, scoreName), scoreEntry);
        EXPECT_GT(indexOfText(texts, averageName), scoreEntry);
        EXPECT_LT(indexOfText(texts, scoreName), accuracyEntry);
        EXPECT_LT(indexOfText(texts, averageName), accuracyEntry);
        EXPECT_GT(indexOfText(texts, accuracyName), accuracyEntry);
        EXPECT_LT(indexOfText(texts, accuracyName), killsEntry);
        EXPECT_GT(indexOfText(texts, killsName), killsEntry);
    }

    TEST(GraphCaptureLayout, LongContextWrapsIntoStoredLines) {
        const GraphSceneAxis yAxis = axis(0.0, 100.0, QStringLiteral("0"), QStringLiteral("100"));
        QList<GraphSceneSeries> lines;
        for (int column = 1; column <= 8; ++column) {
            lines.append(scaledSeries(column, QStringLiteral("Long series name number %1 with a wide label").arg(column),
                                      QColor::fromHsv(column * 40, 200, 220), yAxis, 7, QStringLiteral("Damage")));
        }
        GraphCaptureSnapshot snapshot = snapshotOf(lines);

        const QFontMetricsF titleMetrics(captureFont(snapshot.appearance.font, CaptureTextRole::Title));
        QStringList words;
        while (titleMetrics.horizontalAdvance(words.join(QLatin1Char(' '))) <= kLogical.width() * 1.5) {
            words.append(QStringLiteral("Marathon%1").arg(words.size()));
        }
        snapshot.context.title = words.join(QLatin1Char(' '));

        const CaptureLayout layout = layoutOf(snapshot, kLogical);

        const std::vector<CaptureTextItem> titleLines = captureTextItems(layout.items, CaptureTextRole::Title);
        ASSERT_GT(titleLines.size(), 1u);
        EXPECT_EQ(joinedCaptureText(layout.items, CaptureTextRole::Title).simplified(), snapshot.context.title);
        for (const CaptureTextItem &line : titleLines) {
            EXPECT_GE(line.rect.width(), titleMetrics.horizontalAdvance(line.text)) << line.text.toStdString();
        }

        QSet<qreal> legendRows;
        for (const CaptureGlyphItem &glyph : glyphItems(layout)) legendRows.insert(glyph.rect.top());
        EXPECT_GT(legendRows.size(), 1);

        const std::vector<CaptureTextItem> texts = captureTextItems(layout.items);
        for (const GraphSceneSeries &line : snapshot.scene.series) {
            EXPECT_GE(indexOfText(texts, line.name), 0) << line.name.toStdString();
        }

        const QRectF plot = plotRect(layout, snapshot);
        EXPECT_GE(plot.width(), testComposition().minimumPlot.width());
        EXPECT_GE(plot.height(), testComposition().minimumPlot.height());
    }

    TEST(GraphCaptureLayout, TooLittleRoomIsRejectedForAnOrdinaryGraph) {
        const GraphCaptureSnapshot snapshot = ordinarySnapshot();
        const CaptureComposition composition = testComposition();
        ASSERT_FALSE(snapshot.keepEveryYLabel);

        const QList<QSizeF> crampedSizes = {QSizeF(kLogical.width(), composition.minimumPlot.height()),
                                            QSizeF(composition.minimumPlot.width(), kLogical.height())};
        for (const QSizeF &cramped : crampedSizes) {
            const std::variant<CaptureLayout, CaptureRejection> result =
                    layoutGraphCapture(snapshot, cramped, composition);

            const auto *rejection = std::get_if<CaptureRejection>(&result);
            ASSERT_NE(rejection, nullptr);
            EXPECT_EQ(rejection->reason, CaptureRejectionReason::TooLittleRoom);
            EXPECT_EQ(rejection->correction, CaptureCorrection::LargerDimensions);
        }
    }

    TEST(GraphCaptureLayout, LayoutIsDeterministicAndRenderingLeavesItsInputUntouched) {
        const GraphCaptureSnapshot snapshot = ordinarySnapshot();

        const CaptureLayout first = layoutOf(snapshot, kLogical);
        const CaptureLayout second = layoutOf(snapshot, kLogical);

        ASSERT_FALSE(first.items.empty());
        EXPECT_TRUE(sameItems(first.items, second.items));

        for (const CaptureLayout *layout : {&first, &second}) {
            const CaptureLayout before = *layout;
            for (const int percent : {100, 300}) {
                const QSize pixels = captureOutputPixels(layout->size, percent).value();
                const std::optional<application::RasterImage> image = renderCapture(*layout, pixels);
                ASSERT_TRUE(image.has_value());
                EXPECT_EQ(QSize(image->width(), image->height()), pixels);
                EXPECT_TRUE(sameItems(before.items, layout->items));
                EXPECT_EQ(before.size, layout->size);
                EXPECT_EQ(before.scene, layout->scene);
            }
        }
    }

    namespace {
        constexpr qsizetype kTierCount = 12;

        // Small enough that twelve tier labels fit a 300-high page but not a 150-high one.
        constexpr int kRankFontPixelSize = 6;

        GraphCaptureSnapshot rankSnapshot(const bool keepEveryYLabel, const qsizetype tierCount = kTierCount) {
            GraphSceneAxis yAxis;
            yAxis.min = 0.0;
            yAxis.max = static_cast<qreal>(tierCount - 1);
            for (qsizetype tier = 0; tier < tierCount; ++tier) {
                yAxis.ticks.append(static_cast<qreal>(tier));
                yAxis.labels.append(QStringLiteral("Tier %1").arg(tier + 1));
            }
            yAxis.minLabel = yAxis.labels.first();
            yAxis.maxLabel = yAxis.labels.last();

            GraphCaptureSnapshot snapshot = snapshotOf({series(1, QStringLiteral("Rank"), Qt::red, 2.0, yAxis)});
            snapshot.keepEveryYLabel = keepEveryYLabel;
            return snapshot;
        }

        // The plot minimum leaves rank labels as the only thing that can reject the sizes these cases use.
        CaptureComposition looseComposition() {
            CaptureComposition composition = testComposition();
            composition.minimumPlot = QSizeF(100.0, 20.0);
            return composition;
        }

        CaptureLimits generousLimits() {
            CaptureLimits limits;
            limits.maxEdgePixels = 16000;
            limits.maxPixelArea = 256'000'000;
            limits.maxEstimatedBytes = 8'000'000'000;
            limits.maxSourcePoints = 1'000'000;
            limits.thumbnailBytes = 1'000'000;
            return limits;
        }

        qsizetype placedYLabels(const CaptureLayout &layout, const GraphCaptureSnapshot &snapshot) {
            const std::vector<CaptureGraphItem> graphs = graphItems(layout);
            if (graphs.size() != 1) {
                ADD_FAILURE() << "expected exactly one graph item, found " << graphs.size();
                return 0;
            }
            AxisPainter::Style style;
            style.font = captureFont(layout.font, CaptureTextRole::Tick);
            const GraphSceneAxis &yAxis = *snapshot.scene.labelledYAxis;
            return AxisPainter::layoutLabels(plotRect(layout, snapshot), graphs.front().rect,
                                             AxisPainter::Orientation::Vertical, yAxis.min, yAxis.max, yAxis.ticks,
                                             [&yAxis](const qreal tick) { return yAxis.labelFor(tick); }, style)
                    .size();
        }
    }

    TEST(GraphCaptureRank, TooFewPlacementsFailsValidation) {
        const QSizeF cramped(960.0, 280.0);
        const CaptureComposition composition = looseComposition();
        const CaptureLayout thinned = layoutOf(rankSnapshot(false), cramped, composition);
        ASSERT_LT(placedYLabels(thinned, rankSnapshot(false)), kTierCount);

        const std::variant<CaptureLayout, CaptureRejection> result =
                layoutGraphCapture(rankSnapshot(true), cramped, composition);

        const auto *rejection = std::get_if<CaptureRejection>(&result);
        ASSERT_NE(rejection, nullptr);
        EXPECT_EQ(rejection->reason, CaptureRejectionReason::TooLittleRoom);
        EXPECT_EQ(rejection->correction, CaptureCorrection::LargerDimensions);
    }

    TEST(GraphCaptureRank, ALowerResolutionIsSuggestedOnlyAfterItValidates) {
        GraphCaptureSnapshot thinning = rankSnapshot(false);
        thinning.appearance.font.pixelSize = kRankFontPixelSize;
        GraphCaptureSnapshot ranked = rankSnapshot(true);
        ranked.appearance.font.pixelSize = kRankFontPixelSize;
        const CaptureComposition composition = looseComposition();
        const QSize pixels(960, 300);

        // 200% is the first mark below 300%, so a walk that trusted it would stop one mark too soon.
        ASSERT_LT(placedYLabels(layoutOf(thinning, QSizeF(480.0, 150.0), composition), thinning), kTierCount);
        ASSERT_EQ(placedYLabels(layoutOf(thinning, QSizeF(pixels), composition), thinning), kTierCount);

        const std::optional<CaptureRejection> suggestion =
                suggestGraphResolution(ranked, pixels, 300, composition, generousLimits());

        ASSERT_TRUE(suggestion.has_value());
        EXPECT_EQ(suggestion->correction, CaptureCorrection::LowerResolution);
        EXPECT_EQ(suggestion->suggestedPercent, 100);
        EXPECT_EQ(placedYLabels(layoutOf(ranked, QSizeF(pixels), composition), ranked), kTierCount);
    }

    TEST(GraphCaptureRank, WithNoWorkingResolutionItAsksForLargerDimensions) {
        const std::optional<CaptureRejection> suggestion =
                suggestGraphResolution(rankSnapshot(true), QSize(60, 40), 200, testComposition(), generousLimits());

        ASSERT_TRUE(suggestion.has_value());
        EXPECT_EQ(suggestion->correction, CaptureCorrection::LargerDimensions);
        EXPECT_FALSE(suggestion->suggestedPercent.has_value());
    }

    TEST(GraphCaptureRank, TooMuchSourceDataIsReportedNotRetried) {
        const GraphSceneAxis yAxis = axis(0.0, 100.0, QStringLiteral("0"), QStringLiteral("100"));
        QList<GraphSceneSeries> lines = {series(1, QStringLiteral("First"), Qt::red, 2.0, yAxis),
                                         series(2, QStringLiteral("Second"), Qt::blue, 2.0, yAxis)};
        for (GraphSceneSeries &line : lines) {
            line.observations.clear();
            for (int x = 0; x < 10; ++x) line.observations.append(QPointF(x, 50.0));
        }
        lines.first().trailingCarry = QPointF(10.0, 50.0);
        const GraphCaptureSnapshot snapshot = snapshotOf(lines);

        const qint64 sourcePoints = captureSourcePoints(snapshot);

        ASSERT_EQ(sourcePoints, 21);

        CaptureLimits limits = generousLimits();
        limits.maxSourcePoints = sourcePoints - 1;
        const std::optional<CaptureRejection> rejection =
                suggestGraphResolution(snapshot, QSize(960, 600), 200, testComposition(), limits);

        ASSERT_TRUE(rejection.has_value());
        EXPECT_EQ(rejection->reason, CaptureRejectionReason::SourceGeometry);
        EXPECT_EQ(rejection->correction, CaptureCorrection::TooMuchData);
        EXPECT_FALSE(rejection->suggestedPercent.has_value());
    }

    TEST(GraphCaptureRank, EveryTierLabelFitsAtTheReferenceHeight) {
        const GraphCaptureSnapshot snapshot = rankSnapshot(true);

        const CaptureLayout layout = layoutOf(snapshot, kLogical);

        EXPECT_EQ(placedYLabels(layout, snapshot), kTierCount);
    }

    TEST(GraphCaptureRank, OrdinaryAxesMayThinTheirLabels) {
        const GraphCaptureSnapshot snapshot = rankSnapshot(false);

        const CaptureLayout layout = layoutOf(snapshot, QSizeF(960.0, 280.0), looseComposition());

        EXPECT_LT(placedYLabels(layout, snapshot), kTierCount);
        ASSERT_NE(layout.scene, nullptr);
        EXPECT_EQ(layout.scene->labelledYAxis->min, snapshot.scene.labelledYAxis->min);
        EXPECT_EQ(layout.scene->labelledYAxis->max, snapshot.scene.labelledYAxis->max);
    }

    namespace {
        class ReferenceVm : public presentation::GraphViewModelBase {
        public:
            ReferenceVm() {
                m_xAxis.setRange(0.0, 10.0);
                m_series = new presentation::SeriesModel(this);
                m_series->setColumn(1);
                m_series->setId(QStringLiteral("score"));
                m_series->setName(QStringLiteral("Score"));
                m_series->setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0), QPointF(10.0, 30.0)});
            }

            [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &columns) const override {
                return columns.contains(m_series->column()) ? QList{m_series} : QList<presentation::SeriesModel *>{};
            }

            [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
            [[nodiscard]] int yAxisColumn() const override { return 1; }

        private:
            presentation::ValueAxis m_xAxis;
            presentation::SeriesModel *m_series = nullptr;
        };

        std::optional<GraphCaptureSnapshot> captureAtCanvasSize(ReferenceVm &vm, const QSizeF &canvasSize) {
            GraphCanvas canvas;
            canvas.setSize(canvasSize);
            canvas.setGraphVm(&vm);
            canvas.setVisibleColumns({1});
            return captureGraph(CaptureTarget::Run, canvas, ordinarySnapshot().context, ordinarySnapshot().appearance);
        }

        GraphCaptureSnapshot crowdedSnapshot(const qreal standardWidth) {
            const GraphSceneAxis yAxis = axis(0.0, 100.0, QStringLiteral("0"), QStringLiteral("100"));
            QList<GraphSceneSeries> lines;
            for (int column = 1; column <= 8; ++column) {
                lines.append(scaledSeries(column, QStringLiteral("Long series name number %1 with a wide label").arg(column),
                                          QColor::fromHsv(column * 40, 200, 220), yAxis, 7, QStringLiteral("Damage")));
            }
            GraphCaptureSnapshot snapshot = snapshotOf(lines);

            const QFontMetricsF titleMetrics(captureFont(snapshot.appearance.font, CaptureTextRole::Title));
            QStringList words;
            while (titleMetrics.horizontalAdvance(words.join(QLatin1Char(' '))) <= standardWidth * 12.0) {
                words.append(QStringLiteral("Marathon%1").arg(words.size()));
            }
            snapshot.context.title = words.join(QLatin1Char(' '));
            return snapshot;
        }
    }

    TEST(GraphCaptureReference, IsTheStandardSizeForOrdinaryContent) {
        const CaptureComposition composition = testComposition();
        ReferenceVm vm;

        const std::optional<GraphCaptureSnapshot> small = captureAtCanvasSize(vm, QSizeF(400.0, 200.0));
        const std::optional<GraphCaptureSnapshot> large = captureAtCanvasSize(vm, QSizeF(1600.0, 900.0));
        ASSERT_TRUE(small.has_value());
        ASSERT_TRUE(large.has_value());

        const QSizeF fromSmall = graphReferenceSize(*small, composition);
        const QSizeF fromLarge = graphReferenceSize(*large, composition);

        ASSERT_EQ(fromSmall, composition.standardGraph);
        EXPECT_EQ(fromLarge, composition.standardGraph);
        EXPECT_TRUE(std::holds_alternative<CaptureLayout>(layoutGraphCapture(*small, fromSmall, composition)));
    }

    TEST(GraphCaptureReference, ManyTiersEnlargeItUntilEveryLabelFits) {
        constexpr qsizetype tierCount = 40;
        const GraphCaptureSnapshot snapshot = rankSnapshot(true, tierCount);
        const CaptureComposition composition = testComposition();
        ASSERT_TRUE(std::holds_alternative<CaptureRejection>(
                layoutGraphCapture(snapshot, composition.standardGraph, composition)));

        const QSizeF reference = graphReferenceSize(snapshot, composition);

        ASSERT_GT(reference.height(), composition.standardGraph.height());
        EXPECT_EQ(reference.width(), composition.standardGraph.width());
        EXPECT_EQ(placedYLabels(layoutOf(snapshot, reference), snapshot), tierCount);
    }

    TEST(GraphCaptureReference, AnEnlargedSizeIsWholePixelsSoResolutionDoublesExactly) {
        const CaptureComposition composition = testComposition();
        const GraphCaptureSnapshot snapshot = rankSnapshot(true, 40);

        const QSizeF reference = graphReferenceSize(snapshot, composition);

        ASSERT_GT(reference.height(), composition.standardGraph.height());
        EXPECT_EQ(reference.width(), std::round(reference.width()));
        EXPECT_EQ(reference.height(), std::round(reference.height()));
        const auto at100 = captureOutputPixels(reference, 100);
        const auto at200 = captureOutputPixels(reference, 200);
        ASSERT_TRUE(at100.has_value());
        ASSERT_TRUE(at200.has_value());
        EXPECT_EQ(*at200, *at100 * 2);
    }

    TEST(GraphCaptureReference, LongHeadingsAndLegendsEnlargeIt) {
        const CaptureComposition composition = testComposition();
        const GraphCaptureSnapshot snapshot = crowdedSnapshot(composition.standardGraph.width());
        ASSERT_TRUE(std::holds_alternative<CaptureRejection>(
                layoutGraphCapture(snapshot, composition.standardGraph, composition)));

        const QSizeF reference = graphReferenceSize(snapshot, composition);

        ASSERT_GT(reference.height(), composition.standardGraph.height());
        const QRectF plot = plotRect(layoutOf(snapshot, reference), snapshot);
        EXPECT_GE(plot.width(), composition.minimumPlot.width());
        EXPECT_GE(plot.height(), composition.minimumPlot.height());
    }
}
