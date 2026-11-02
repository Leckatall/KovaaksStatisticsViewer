#include <gtest/gtest.h>

#include <QColor>
#include <QFontMetricsF>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

#include "capture/capture_fonts.h"
#include "capture/capture_layout.h"
#include "capture/capture_snapshot.h"
#include "capture/table_capture_layout.h"
#include "capture_test_support.h"
#include "presentation/benchmarks/benchmark_format.h"
#include "table_capture_fixtures.h"

namespace ksv::ui {
    namespace {
        struct RankChip {
            CaptureRectItem rect;
            CaptureTextItem text;
        };

        std::optional<CaptureTextItem> textEqualTo(const CaptureLayout &layout, const QString &text) {
            for (const CaptureTextItem &item : captureTextItems(layout.items)) {
                if (item.text == text) return item;
            }
            return std::nullopt;
        }

        int countOfText(const CaptureLayout &layout, const QString &text) {
            return static_cast<int>(std::ranges::count(captureTextItems(layout.items), text, &CaptureTextItem::text));
        }

        // Every item with this exact text in the horizontal band of `anchor`; rects of neighbouring rows never overlap.
        std::vector<CaptureTextItem> cellsBeside(const CaptureLayout &layout, const CaptureTextItem &anchor,
                                                 const QString &text) {
            std::vector<CaptureTextItem> cells;
            for (const CaptureTextItem &item : captureTextItems(layout.items)) {
                if (item.text == text && item.rect.top() < anchor.rect.bottom() &&
                    anchor.rect.top() < item.rect.bottom()) {
                    cells.push_back(item);
                }
            }
            return cells;
        }

        // The stored lines that spell `full` in reading order, or none when any of its characters is missing.
        std::vector<CaptureTextItem> linesSpelling(const CaptureLayout &layout, const QString &full) {
            std::vector<CaptureTextItem> lines;
            QString remaining = full;
            for (const CaptureTextItem &item : captureTextItems(layout.items)) {
                if (remaining.isEmpty()) break;
                if (item.text.isEmpty() || !remaining.startsWith(item.text)) continue;
                lines.push_back(item);
                remaining = remaining.sliced(item.text.size()).trimmed();
            }
            return remaining.isEmpty() ? lines : std::vector<CaptureTextItem>{};
        }

        QColor tintAt(const QString &color, const qreal alpha) {
            QColor tint(color);
            tint.setAlphaF(alpha);
            return tint;
        }

        // A tinted group's cell, found by the fixture's unique tint rather than by geometry.
        std::optional<QRectF> tintedCell(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout,
                                         const QString &color) {
            const QColor fill = tintAt(color, snapshot.appearance.groupTintAlpha);
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (rect && rect->fill == fill) return rect->rect;
            }
            return std::nullopt;
        }

        // The row's background: the widest base-coloured rect under its scenario name.
        std::optional<QRectF> rowBand(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout,
                                      const CaptureTextItem &name) {
            std::optional<QRectF> widest;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (!rect || rect->fill != snapshot.appearance.base || !rect->rect.contains(name.rect.center())) {
                    continue;
                }
                if (!widest || rect->rect.width() > widest->width()) widest = rect->rect;
            }
            return widest;
        }

        std::vector<CaptureRectItem> roundedOutlines(const CaptureLayout &layout) {
            std::vector<CaptureRectItem> rects;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (rect && rect->border.isValid() && rect->radius > 0.0) rects.push_back(*rect);
            }
            return rects;
        }

        std::optional<RankChip> chipHolding(const CaptureLayout &layout, const QString &text) {
            for (const CaptureRectItem &chip : roundedOutlines(layout)) {
                for (const CaptureTextItem &item : captureTextItems(layout.items)) {
                    if (item.text == text && chip.rect.contains(item.rect)) return RankChip{chip, item};
                }
            }
            return std::nullopt;
        }

        qreal advanceOf(const TableCaptureSnapshot &snapshot, const CaptureTextItem &item) {
            return QFontMetricsF(captureFont(snapshot.appearance.font, item.role)).horizontalAdvance(item.text);
        }

        QStringList plainRowNames(const int count) {
            QStringList names;
            for (int i = 0; i < count; ++i) names.append(QStringLiteral("Drill %1").arg(i));
            return names;
        }

        // Far wider than any plausible identity-column cap, in words that wrap cleanly.
        QString longPhrase(const QString &word) {
            QStringList words;
            for (int i = 0; i < 40; ++i) words.append(word + QString::number(i));
            return words.join(QLatin1Char(' '));
        }

        presentation::ProgressGridGroup wholeTableGroup(const int rows) {
            return tableFixtureGroup(QStringLiteral("Everything"), 0, rows, tableFixtureTint(0));
        }

        TableCaptureSnapshot plainTable(const int rows, const int tiers = 3) {
            return tableFixtureSnapshot(plainRowNames(rows), {wholeTableGroup(rows)}, tiers);
        }

        std::optional<QRectF> bandOfRow(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout,
                                        const int row) {
            const std::optional<CaptureTextItem> name = textEqualTo(layout, snapshot.grid.rows.at(row).name);
            return name ? rowBand(snapshot, layout, *name) : std::nullopt;
        }

        std::vector<CaptureTextItem> cellsInBand(const CaptureLayout &layout, const QRectF &band, const QString &text) {
            std::vector<CaptureTextItem> cells;
            for (const CaptureTextItem &item : captureTextItems(layout.items)) {
                if (item.text == text && band.contains(item.rect.center())) cells.push_back(item);
            }
            return cells;
        }

        // A chunk's track is the only translucent wash of the text colour, whatever its alpha, ordered left to right.
        std::vector<QRectF> chunkTracks(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout,
                                        const QRectF &band) {
            std::vector<QRectF> tracks;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (rect && rect->fill.rgb() == snapshot.appearance.text.rgb() && rect->fill.alpha() < 255 &&
                    band.contains(rect->rect.center())) {
                    tracks.push_back(rect->rect);
                }
            }
            std::ranges::sort(tracks, {}, &QRectF::left);
            return tracks;
        }

        std::vector<QRectF> rectsFilled(const CaptureLayout &layout, const QRectF &band, const QColor &fill) {
            std::vector<QRectF> rects;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (rect && rect->fill == fill && band.contains(rect->rect.center())) rects.push_back(rect->rect);
            }
            return rects;
        }

        // Ordered top to bottom, so a cut marker reads as its upper then its lower piece.
        std::vector<QRectF> markerTicks(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout,
                                        const QRectF &band) {
            std::vector<QRectF> ticks;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (rect && rect->fill == snapshot.appearance.text && rect->border == snapshot.appearance.base &&
                    band.contains(rect->rect.center())) {
                    ticks.push_back(rect->rect);
                }
            }
            std::ranges::sort(ticks, {}, &QRectF::top);
            return ticks;
        }

        std::vector<CaptureHatchItem> hatchesIn(const CaptureLayout &layout, const QRectF &band) {
            std::vector<CaptureHatchItem> hatches;
            for (const CaptureItem &item : layout.items) {
                const auto *hatch = std::get_if<CaptureHatchItem>(&item);
                if (hatch && band.contains(hatch->rect.center())) hatches.push_back(*hatch);
            }
            return hatches;
        }

        std::vector<CaptureGlyphItem> glyphsIn(const CaptureLayout &layout, const QRectF &area,
                                               const CaptureGlyphItem::Kind kind) {
            std::vector<CaptureGlyphItem> glyphs;
            for (const CaptureItem &item : layout.items) {
                const auto *glyph = std::get_if<CaptureGlyphItem>(&item);
                if (glyph && glyph->kind == kind && area.contains(glyph->rect.center())) glyphs.push_back(*glyph);
            }
            return glyphs;
        }

        struct KeyEntries {
            std::size_t explanations = 0;
            std::size_t diamonds = 0;
            std::size_t chevrons = 0;
            std::size_t ticks = 0;
        };

        KeyEntries keyBelow(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout, const QRectF &lastRow) {
            const QRectF below(0.0, lastRow.bottom(), layout.size.width(), layout.size.height() - lastRow.bottom());
            KeyEntries key;
            key.explanations = static_cast<std::size_t>(std::ranges::count_if(
                captureTextItems(layout.items),
                [&](const CaptureTextItem &item) { return below.contains(item.rect.center()); }));
            key.diamonds = glyphsIn(layout, below, CaptureGlyphItem::Kind::Diamond).size();
            key.chevrons = glyphsIn(layout, below, CaptureGlyphItem::Kind::Chevron).size();
            key.ticks = markerTicks(snapshot, layout, below).size();
            return key;
        }

        // The digits' extent inside a value item, which may be as tall as the bar.
        qreal digitHalfHeight(const TableCaptureSnapshot &snapshot, const CaptureTextItem &value) {
            return QFontMetricsF(captureFont(snapshot.appearance.font, value.role)).capHeight() / 2.0;
        }

        TableCaptureSnapshot summarisedTable() {
            TableCaptureSnapshot snapshot = plainTable(3);
            snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
            return snapshot;
        }

        std::optional<qreal> tableTop(const TableCaptureSnapshot &snapshot, const CaptureLayout &layout) {
            const std::optional<CaptureTextItem> heading = textEqualTo(layout, snapshot.grid.headings.scenario);
            return heading ? std::optional(heading->rect.top()) : std::nullopt;
        }

        std::vector<CaptureItem> itemsFrom(const CaptureLayout &layout, const qreal top) {
            std::vector<CaptureItem> items;
            for (const CaptureItem &item : layout.items) {
                if (std::visit([](const auto &held) { return held.rect.top(); }, item) >= top) items.push_back(item);
            }
            return items;
        }

        std::vector<QString> summaryTexts(const TableCaptureSummary &summary) {
            return {summary.captions.attainedRank, summary.captions.completedRank, summary.captions.averageRank,
                    summary.captions.totalPlaytime, summary.captions.nextTier, summary.attainedRank,
                    summary.completedRank, summary.averageRank, summary.totalPlaytime, summary.nextTier,
                    summary.nextTierBlockerText};
        }
    }

    TEST(TableCaptureLayout, HoldsEveryHeadingRowAndValue) {
        const TableCaptureSnapshot snapshot = largeTableFixture();
        const presentation::ProgressGridContent &grid = snapshot.grid;

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        for (const QString &heading : {grid.headings.category, grid.headings.subcategory, grid.headings.scenario,
                                       grid.headings.pbDate, grid.headings.plays, grid.headings.next}) {
            ASSERT_EQ(countOfText(layout, heading), 1) << heading.toStdString();
        }
        EXPECT_EQ(countOfText(layout, grid.headings.progress), 0);

        const CaptureTextItem scenarioHeading = *textEqualTo(layout, grid.headings.scenario);
        for (const presentation::ProgressGridTier &tier : grid.tiers) {
            EXPECT_EQ(cellsBeside(layout, scenarioHeading, tier.name).size(), 1u) << tier.name.toStdString();
        }
        for (const presentation::ProgressGridGroup &group : grid.groups) {
            EXPECT_EQ(countOfText(layout, group.name), 1) << group.name.toStdString();
        }

        for (const presentation::ProgressGridScenarioRow &row : grid.rows) {
            ASSERT_EQ(countOfText(layout, row.name), 1) << row.name.toStdString();
            const CaptureTextItem name = *textEqualTo(layout, row.name);
            EXPECT_EQ(cellsBeside(layout, name, row.personalBestDateText).size(), 1u) << row.name.toStdString();
            EXPECT_EQ(cellsBeside(layout, name, row.playCountText).size(), 1u) << row.name.toStdString();
            EXPECT_EQ(cellsBeside(layout, name, row.nextThresholdText).size(), 1u) << row.name.toStdString();
            if (row.nextTierText.isEmpty()) continue;

            // The live cell is two labels, and the tier's half carries the tier's colour.
            const std::vector<CaptureTextItem> nextTier = cellsBeside(layout, name, row.nextTierText);
            ASSERT_EQ(nextTier.size(), 1u) << row.name.toStdString();
            EXPECT_EQ(nextTier.front().color, QColor(row.nextTierColor));
        }

        const QRectF canvas(QPointF(0.0, 0.0), layout.size);
        for (const CaptureItem &item : layout.items) {
            const QRectF rect = std::visit([](const auto &held) { return held.rect; }, item);
            EXPECT_TRUE(canvas.contains(rect));
        }
        for (const CaptureTextItem &item : captureTextItems(layout.items)) {
            EXPECT_GE(item.rect.width(), advanceOf(snapshot, item)) << item.text.toStdString();
        }
    }

    TEST(TableCaptureLayout, ThereIsOneIdentityColumnSetAndNoPinOffset) {
        using presentation::ProgressGridGroupColumn;

        const QString plainCategoryTint = tableFixtureTint(0);
        const QString spanningTint = tableFixtureTint(1);
        const QString subcategoryTint = tableFixtureTint(2);

        presentation::ProgressGridGroup spanning =
            tableFixtureGroup(QStringLiteral("Spanning"), 4, 2, spanningTint);
        spanning.spansSubcategory = true;
        const TableCaptureSnapshot withSubcategories = tableFixtureSnapshot(
            plainRowNames(6),
            {tableFixtureGroup(QStringLiteral("Plain"), 0, 4, plainCategoryTint), spanning,
             tableFixtureGroup(QStringLiteral("Sub one"), 0, 2, subcategoryTint, ProgressGridGroupColumn::Subcategory),
             tableFixtureGroup(QStringLiteral("Sub two"), 2, 2, tableFixtureTint(3),
                               ProgressGridGroupColumn::Subcategory)});
        const TableCaptureSnapshot withoutSubcategories = tableFixtureSnapshot(
            plainRowNames(6), {tableFixtureGroup(QStringLiteral("Everything"), 0, 6, plainCategoryTint)});

        const CaptureLayout both = layoutTableCapture(withSubcategories, false);
        const CaptureLayout categoryOnly = layoutTableCapture(withoutSubcategories, false);

        const presentation::ProgressGridHeadings &headings = withSubcategories.grid.headings;
        ASSERT_EQ(countOfText(both, headings.category), 1);
        EXPECT_EQ(countOfText(both, headings.subcategory), 1);
        EXPECT_EQ(countOfText(both, headings.scenario), 1);

        EXPECT_EQ(countOfText(categoryOnly, headings.category), 1);
        EXPECT_EQ(countOfText(categoryOnly, headings.subcategory), 0);
        EXPECT_EQ(countOfText(categoryOnly, headings.scenario), 1);

        const std::optional<QRectF> plainCell = tintedCell(withSubcategories, both, plainCategoryTint);
        const std::optional<QRectF> spanningCell = tintedCell(withSubcategories, both, spanningTint);
        const std::optional<QRectF> subcategoryCell = tintedCell(withSubcategories, both, subcategoryTint);
        ASSERT_TRUE(plainCell && spanningCell && subcategoryCell);
        EXPECT_DOUBLE_EQ(spanningCell->left(), plainCell->left());
        EXPECT_DOUBLE_EQ(spanningCell->right(), subcategoryCell->right());
        EXPECT_GT(spanningCell->width(), plainCell->width());
    }

    TEST(TableCaptureLayout, ALongNameWrapsAndItsRowGrows) {
        const QString longName = longPhrase(QStringLiteral("Wrapping"));
        const QString unbreakable = QStringLiteral("Unbreakable").repeated(8);
        const QString plainName = QStringLiteral("Plain drill");

        const TableCaptureSnapshot cappedOnly = tableFixtureSnapshot(
            {longName, plainName}, {tableFixtureGroup(QStringLiteral("Group"), 0, 2, tableFixtureTint(0))});
        const TableCaptureSnapshot widened = tableFixtureSnapshot(
            {longName, plainName, unbreakable}, {tableFixtureGroup(QStringLiteral("Group"), 0, 3, tableFixtureTint(0))});

        const CaptureLayout capped = layoutTableCapture(cappedOnly, false);
        const CaptureLayout layout = layoutTableCapture(widened, false);

        const std::vector<CaptureTextItem> lines = linesSpelling(layout, longName);
        ASSERT_GT(lines.size(), 1u);

        const std::optional<QRectF> longRow = rowBand(widened, layout, lines.front());
        const std::optional<CaptureTextItem> plain = textEqualTo(layout, plainName);
        ASSERT_TRUE(longRow && plain);
        const std::optional<QRectF> plainRow = rowBand(widened, layout, *plain);
        ASSERT_TRUE(plainRow);
        EXPECT_GT(longRow->height(), plainRow->height());

        qreal lineTop = longRow->top();
        for (const CaptureTextItem &line : lines) {
            EXPECT_TRUE(longRow->contains(line.rect)) << line.text.toStdString();
            EXPECT_GE(line.rect.top(), lineTop) << line.text.toStdString();
            EXPECT_GE(line.rect.width(), advanceOf(widened, line)) << line.text.toStdString();
            lineTop = line.rect.bottom();
        }

        ASSERT_EQ(countOfText(layout, unbreakable), 1);
        const CaptureTextItem word = *textEqualTo(layout, unbreakable);
        EXPECT_GE(word.rect.width(), advanceOf(widened, word));

        // The capped table's column is the widest the cap allows, so the word has to outgrow that.
        const std::vector<CaptureTextItem> cappedLines = linesSpelling(capped, longName);
        ASSERT_FALSE(cappedLines.empty());
        const qreal widestCappedLine = std::ranges::max(
            cappedLines | std::views::transform([&](const CaptureTextItem &line) {
                return advanceOf(cappedOnly, line);
            }));
        ASSERT_GT(advanceOf(widened, word), widestCappedLine);

        const QString nextColumn = widened.grid.headings.pbDate;
        const std::optional<CaptureTextItem> nextHeadingCapped = textEqualTo(capped, nextColumn);
        const std::optional<CaptureTextItem> nextHeadingWidened = textEqualTo(layout, nextColumn);
        ASSERT_TRUE(nextHeadingCapped && nextHeadingWidened);
        EXPECT_GT(nextHeadingWidened->rect.left(), nextHeadingCapped->rect.left());
    }

    TEST(TableCaptureLayout, GroupSpansFollowRowHeights) {
        const QString wrappedRowName = longPhrase(QStringLiteral("Wrapping"));
        const QString wrappedGroupName = longPhrase(QStringLiteral("Grouping"));
        const QStringList rowNames{wrappedRowName, QStringLiteral("Beside"), QStringLiteral("Lonely"),
                                   QStringLiteral("Outside one"), QStringLiteral("Outside two")};
        const QString wrappedRowTint = tableFixtureTint(0);
        const QString oneRowTint = tableFixtureTint(1);
        const QString outsideTint = tableFixtureTint(2);

        presentation::ProgressGridGroup oneRow = tableFixtureGroup(wrappedGroupName, 2, 1, oneRowTint);
        oneRow.rankText = tableFixtureTiers(3).at(1).name;
        oneRow.rankColor = tableFixtureTiers(3).at(1).color;
        oneRow.rankBarColor = tableFixtureTiers(3).at(1).barColor;

        const TableCaptureSnapshot snapshot = tableFixtureSnapshot(
            rowNames, {tableFixtureGroup(QStringLiteral("Two rows"), 0, 2, wrappedRowTint), oneRow,
                       tableFixtureGroup(QStringLiteral("Outside"), 3, 2, outsideTint)});
        const TableCaptureSnapshot baseline = tableFixtureSnapshot(
            plainRowNames(5), {tableFixtureGroup(QStringLiteral("Everything"), 0, 5, tableFixtureTint(3))});

        const CaptureLayout layout = layoutTableCapture(snapshot, false);
        const CaptureLayout baseLayout = layoutTableCapture(baseline, false);

        const std::vector<CaptureTextItem> nameLines = linesSpelling(layout, wrappedRowName);
        ASSERT_FALSE(nameLines.empty());
        const std::optional<CaptureTextItem> beside = textEqualTo(layout, rowNames.at(1));
        const std::optional<CaptureTextItem> lonely = textEqualTo(layout, rowNames.at(2));
        const std::optional<CaptureTextItem> outsideOne = textEqualTo(layout, rowNames.at(3));
        const std::optional<CaptureTextItem> outsideTwo = textEqualTo(layout, rowNames.at(4));
        const std::optional<CaptureTextItem> baseName = textEqualTo(baseLayout, baseline.grid.rows.front().name);
        ASSERT_TRUE(beside && lonely && outsideOne && outsideTwo && baseName);

        const std::optional<QRectF> firstBand = rowBand(snapshot, layout, nameLines.front());
        const std::optional<QRectF> besideBand = rowBand(snapshot, layout, *beside);
        const std::optional<QRectF> lonelyBand = rowBand(snapshot, layout, *lonely);
        const std::optional<QRectF> outsideOneBand = rowBand(snapshot, layout, *outsideOne);
        const std::optional<QRectF> outsideTwoBand = rowBand(snapshot, layout, *outsideTwo);
        const std::optional<QRectF> baseBand = rowBand(baseline, baseLayout, *baseName);
        ASSERT_TRUE(firstBand && besideBand && lonelyBand && outsideOneBand && outsideTwoBand && baseBand);

        const std::optional<QRectF> twoRowCell = tintedCell(snapshot, layout, wrappedRowTint);
        const std::optional<QRectF> oneRowCell = tintedCell(snapshot, layout, oneRowTint);
        const std::optional<QRectF> outsideCell = tintedCell(snapshot, layout, outsideTint);
        ASSERT_TRUE(twoRowCell && oneRowCell && outsideCell);

        EXPECT_GT(firstBand->height(), baseBand->height());
        EXPECT_DOUBLE_EQ(twoRowCell->top(), firstBand->top());
        EXPECT_DOUBLE_EQ(twoRowCell->bottom(), besideBand->bottom());
        EXPECT_DOUBLE_EQ(oneRowCell->top(), lonelyBand->top());
        EXPECT_DOUBLE_EQ(oneRowCell->bottom(), lonelyBand->bottom());
        EXPECT_DOUBLE_EQ(outsideCell->top(), outsideOneBand->top());
        EXPECT_DOUBLE_EQ(outsideCell->bottom(), outsideTwoBand->bottom());

        EXPECT_GT(lonelyBand->height(), baseBand->height());
        const std::vector<CaptureTextItem> groupLines = linesSpelling(layout, wrappedGroupName);
        ASSERT_GT(groupLines.size(), 1u);
        for (const CaptureTextItem &line : groupLines) {
            EXPECT_TRUE(lonelyBand->contains(line.rect)) << line.text.toStdString();
        }
        const std::optional<RankChip> chip = chipHolding(layout, oneRow.rankText);
        ASSERT_TRUE(chip);
        EXPECT_TRUE(lonelyBand->contains(chip->rect.rect));

        EXPECT_DOUBLE_EQ(outsideOneBand->height(), baseBand->height());
        EXPECT_DOUBLE_EQ(outsideTwoBand->height(), baseBand->height());
    }

    TEST(TableCaptureLayout, GroupRanksAndTintsAreKept) {
        const std::vector<presentation::ProgressGridTier> tiers = tableFixtureTiers(3);
        const presentation::ProgressGridTier &rank = tiers.at(1);
        const QString rankedTint = tableFixtureTint(0);

        presentation::ProgressGridGroup ranked = tableFixtureGroup(QStringLiteral("Ranked"), 0, 1, rankedTint);
        ranked.rankText = rank.name;
        ranked.rankColor = rank.color;
        ranked.rankBarColor = rank.barColor;
        presentation::ProgressGridGroup unranked =
            tableFixtureGroup(QStringLiteral("Not ranked"), 1, 1, tableFixtureTint(1));
        unranked.rankText = QStringLiteral("Unranked");
        const presentation::ProgressGridGroup uncategorized =
            tableFixtureGroup(QStringLiteral("Uncategorized"), 2, 1, QString());

        const TableCaptureSnapshot snapshot =
            tableFixtureSnapshot(plainRowNames(3), {ranked, unranked, uncategorized});

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<RankChip> rankedChip = chipHolding(layout, ranked.rankText);
        ASSERT_TRUE(rankedChip);
        EXPECT_EQ(rankedChip->rect.border, QColor(rank.barColor));
        EXPECT_EQ(rankedChip->text.color, QColor(rank.color));

        const std::optional<RankChip> unrankedChip = chipHolding(layout, unranked.rankText);
        ASSERT_TRUE(unrankedChip);
        EXPECT_EQ(unrankedChip->rect.border, snapshot.appearance.mid);
        EXPECT_EQ(unrankedChip->text.color, snapshot.appearance.placeholderText);

        EXPECT_EQ(countOfText(layout, uncategorized.name), 1);
        const std::optional<CaptureTextItem> uncategorizedRow = textEqualTo(layout, snapshot.grid.rows.at(2).name);
        const std::optional<CaptureTextItem> scenarioHeading = textEqualTo(layout, snapshot.grid.headings.scenario);
        ASSERT_TRUE(uncategorizedRow && scenarioHeading);
        const std::optional<QRectF> band = rowBand(snapshot, layout, *uncategorizedRow);
        ASSERT_TRUE(band);
        const QRectF identityArea(band->left(), band->top(), scenarioHeading->rect.left() - band->left(),
                                  band->height());
        EXPECT_TRUE(std::ranges::none_of(roundedOutlines(layout), [&](const CaptureRectItem &chip) {
            return chip.rect.intersects(identityArea);
        }));

        EXPECT_TRUE(tintedCell(snapshot, layout, rankedTint).has_value());
    }

    // The source can format an empty value any way it likes, and a real value may look like the placeholder, so
    // the style has to come from the typed flags.
    TEST(TableCaptureLayout, AnEmptyValueIsPlaceholderColouredWithoutComparingText) {
        TableCaptureSnapshot snapshot = tableFixtureSnapshot(
            plainRowNames(2), {tableFixtureGroup(QStringLiteral("Everything"), 0, 2, tableFixtureTint(0))});
        presentation::ProgressGridScenarioRow &absent = snapshot.grid.rows.at(0);
        absent.hasPersonalBestDate = false;
        absent.hasPlayCount = false;
        absent.personalBestDateText = QStringLiteral("unrelated date text");
        absent.playCountText = QStringLiteral("unrelated play text");
        presentation::ProgressGridScenarioRow &present = snapshot.grid.rows.at(1);
        present.personalBestDateText = QStringLiteral("—");
        present.playCountText = QStringLiteral("—");

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<CaptureTextItem> absentDate = textEqualTo(layout, absent.personalBestDateText);
        const std::optional<CaptureTextItem> absentPlays = textEqualTo(layout, absent.playCountText);
        ASSERT_TRUE(absentDate && absentPlays);
        EXPECT_EQ(absentDate->color, snapshot.appearance.placeholderText);
        EXPECT_EQ(absentPlays->color, snapshot.appearance.placeholderText);

        const std::optional<CaptureTextItem> presentName = textEqualTo(layout, present.name);
        ASSERT_TRUE(presentName);
        const std::vector<CaptureTextItem> presentValues = cellsBeside(layout, *presentName, QStringLiteral("—"));
        ASSERT_EQ(presentValues.size(), 2u);
        for (const CaptureTextItem &value : presentValues) {
            EXPECT_EQ(value.color, snapshot.appearance.text);
        }
    }

    TEST(TableCaptureBars, AChunkIsAsWideAsItsTierNameOrWidestThreshold) {
        TableCaptureSnapshot snapshot = plainTable(3);
        snapshot.grid.tiers.at(0).name = QStringLiteral("Grandmaster Extraordinaire");
        snapshot.grid.tiers.at(1).name = QStringLiteral("Bo");
        snapshot.grid.tiers.at(2).name = QStringLiteral("Cy");
        for (presentation::ProgressGridScenarioRow &row : snapshot.grid.rows) {
            row.thresholdTexts = {QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30")};
        }
        snapshot.grid.rows.at(1).thresholdTexts.at(1) = QStringLiteral("1,234,567,890.125");

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<QRectF> firstBand = bandOfRow(snapshot, layout, 0);
        ASSERT_TRUE(firstBand);
        const std::vector<QRectF> chunks = chunkTracks(snapshot, layout, *firstBand);
        ASSERT_EQ(chunks.size(), 3u);

        EXPECT_GT(chunks.at(0).width(), chunks.at(2).width());
        EXPECT_GT(chunks.at(1).width(), chunks.at(2).width());

        const std::optional<CaptureTextItem> heading = textEqualTo(layout, snapshot.grid.headings.scenario);
        ASSERT_TRUE(heading);
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            const std::vector<CaptureTextItem> tier = cellsBeside(layout, *heading, snapshot.grid.tiers.at(i).name);
            ASSERT_EQ(tier.size(), 1u);
            EXPECT_NEAR(tier.front().rect.center().x(), chunks.at(i).center().x(), 1.0);
            EXPECT_GE(chunks.at(i).width(), advanceOf(snapshot, tier.front()));
        }

        for (int row = 0; row < 3; ++row) {
            const std::optional<QRectF> band = bandOfRow(snapshot, layout, row);
            ASSERT_TRUE(band);
            const std::vector<QRectF> rowChunks = chunkTracks(snapshot, layout, *band);
            ASSERT_EQ(rowChunks.size(), chunks.size());
            for (std::size_t i = 0; i < chunks.size(); ++i) {
                EXPECT_DOUBLE_EQ(rowChunks.at(i).left(), chunks.at(i).left());
                EXPECT_DOUBLE_EQ(rowChunks.at(i).right(), chunks.at(i).right());

                const std::vector<CaptureTextItem> threshold =
                    cellsInBand(layout, *band, snapshot.grid.rows.at(row).thresholdTexts.at(i));
                ASSERT_EQ(threshold.size(), 1u);
                EXPECT_GE(threshold.front().rect.left(), chunks.at(i).left());
                EXPECT_LE(threshold.front().rect.right(), chunks.at(i).right());
                EXPECT_GE(threshold.front().rect.width(), advanceOf(snapshot, threshold.front()));
            }
        }
    }

    // Live, a threshold under the value or the marker moves or disappears. The lane exists so that neither can
    // happen: the same placement holds wherever the value and the marker land.
    TEST(TableCaptureBars, ThresholdsSitInTheirLaneAlignedToChunks) {
        TableCaptureSnapshot snapshot = plainTable(2);
        presentation::ProgressGridScenarioRow &played = snapshot.grid.rows.at(0);
        played.fills = {1.0, 0.9, 0.0};
        played.recentPosition = 1.95;
        played.thresholdTexts.at(2).clear();

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<QRectF> band = bandOfRow(snapshot, layout, 0);
        ASSERT_TRUE(band);
        const std::vector<CaptureTextItem> firstThreshold = cellsInBand(layout, *band, played.thresholdTexts.at(0));
        ASSERT_EQ(firstThreshold.size(), 1u);

        const std::vector<QRectF> chunks = chunkTracks(snapshot, layout, *band);
        ASSERT_EQ(chunks.size(), 3u);
        const std::vector<CaptureTextItem> value = cellsInBand(layout, *band, played.personalBestText);
        ASSERT_EQ(value.size(), 1u);
        const std::vector<QRectF> ticks = markerTicks(snapshot, layout, *band);
        ASSERT_FALSE(ticks.empty());

        for (std::size_t i = 0; i < 2; ++i) {
            const std::vector<CaptureTextItem> threshold = cellsInBand(layout, *band, played.thresholdTexts.at(i));
            ASSERT_EQ(threshold.size(), 1u);
            const QRectF rect = threshold.front().rect;
            EXPECT_GE(rect.top(), chunks.at(i).bottom());
            EXPECT_TRUE(threshold.front().alignment.testFlag(Qt::AlignRight));
            EXPECT_LE(rect.right(), chunks.at(i).right());
            EXPECT_GT(rect.right(), chunks.at(i).center().x());
            EXPECT_FALSE(rect.intersects(value.front().rect));
            for (const QRectF &tick : ticks) EXPECT_FALSE(rect.intersects(tick));
        }
        EXPECT_TRUE(cellsInBand(layout, *band, QString()).empty());
    }

    TEST(TableCaptureBars, HatchedChunksAndStatusRowsLeaveTheLaneEmpty) {
        TableCaptureSnapshot snapshot = plainTable(3);
        presentation::ProgressGridScenarioRow &hatched = snapshot.grid.rows.at(0);
        hatched.missingChunks.at(1) = true;
        hatched.fills = {0.5, 0.0, 0.0};
        presentation::ProgressGridScenarioRow &unplayed = snapshot.grid.rows.at(1);
        unplayed.edgeState = presentation::ProgressGridEdgeState::Unplayed;
        unplayed.statusText = QStringLiteral("Not played yet");
        presentation::ProgressGridScenarioRow &unmatched = snapshot.grid.rows.at(2);
        unmatched.edgeState = presentation::ProgressGridEdgeState::Unmatched;
        unmatched.statusText = QStringLiteral("Mapping needs fixing");

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<QRectF> hatchedBand = bandOfRow(snapshot, layout, 0);
        const std::optional<QRectF> unplayedBand = bandOfRow(snapshot, layout, 1);
        const std::optional<QRectF> unmatchedBand = bandOfRow(snapshot, layout, 2);
        ASSERT_TRUE(hatchedBand && unplayedBand && unmatchedBand);

        const std::vector<CaptureHatchItem> hatches = hatchesIn(layout, *hatchedBand);
        ASSERT_EQ(hatches.size(), 1u);
        const std::vector<QRectF> chunks = chunkTracks(snapshot, layout, *hatchedBand);
        ASSERT_EQ(chunks.size(), 3u);
        EXPECT_EQ(hatches.front().rect, chunks.at(1));
        EXPECT_TRUE(cellsInBand(layout, *hatchedBand, hatched.thresholdTexts.at(1)).empty());
        EXPECT_EQ(cellsInBand(layout, *hatchedBand, hatched.thresholdTexts.at(0)).size(), 1u);
        EXPECT_EQ(cellsInBand(layout, *hatchedBand, hatched.thresholdTexts.at(2)).size(), 1u);

        const std::vector<CaptureTextItem> unplayedStatus = cellsInBand(layout, *unplayedBand, unplayed.statusText);
        ASSERT_EQ(unplayedStatus.size(), 1u);
        EXPECT_EQ(unplayedStatus.front().color, snapshot.appearance.placeholderText);
        const std::vector<CaptureTextItem> unmatchedStatus = cellsInBand(layout, *unmatchedBand, unmatched.statusText);
        ASSERT_EQ(unmatchedStatus.size(), 1u);
        EXPECT_EQ(unmatchedStatus.front().color, snapshot.appearance.attention);

        const auto statusRows = {std::pair{&unplayed, *unplayedBand}, std::pair{&unmatched, *unmatchedBand}};
        for (const auto &[row, band] : statusRows) {
            EXPECT_TRUE(cellsInBand(layout, band, row->personalBestText).empty());
            for (const QString &threshold : row->thresholdTexts) {
                EXPECT_TRUE(cellsInBand(layout, band, threshold).empty()) << threshold.toStdString();
            }
        }

        const auto outlined = [&](const QRectF &band) {
            return std::ranges::any_of(roundedOutlines(layout), [&](const CaptureRectItem &rect) {
                return rect.border == snapshot.appearance.attention && band.contains(rect.rect.center());
            });
        };
        EXPECT_TRUE(outlined(*unmatchedBand));
        EXPECT_FALSE(outlined(*unplayedBand));
    }

    TEST(TableCaptureBars, TheValueFollowsTheInsideOrPastFillRule) {
        const auto layoutWithTopTier = [](const QString &barColor) {
            TableCaptureSnapshot snapshot = plainTable(3);
            snapshot.grid.tiers.at(2).barColor = barColor;
            snapshot.grid.rows.at(0).fills = {0.2, 0.0, 0.0};
            snapshot.grid.rows.at(1).fills = {1.0, 1.0, 0.95};
            snapshot.grid.rows.at(2).fills = {0.5, 0.0, 0.0};
            snapshot.grid.rows.at(2).hasPersonalBest = false;
            return std::pair{snapshot, layoutTableCapture(snapshot, false)};
        };
        const QString lightBar = QColor(240, 240, 240).name();
        const QString darkBar = QColor(32, 32, 32).name();
        ASSERT_TRUE(presentation::isLightFill(QColor(lightBar)));
        ASSERT_FALSE(presentation::isLightFill(QColor(darkBar)));

        const auto [snapshot, layout] = layoutWithTopTier(lightBar);
        const presentation::ProgressGridScenarioRow &shortRow = snapshot.grid.rows.at(0);
        const presentation::ProgressGridScenarioRow &longRow = snapshot.grid.rows.at(1);
        const presentation::ProgressGridScenarioRow &noBest = snapshot.grid.rows.at(2);

        const std::optional<QRectF> shortBand = bandOfRow(snapshot, layout, 0);
        const std::optional<QRectF> longBand = bandOfRow(snapshot, layout, 1);
        const std::optional<QRectF> noBestBand = bandOfRow(snapshot, layout, 2);
        ASSERT_TRUE(shortBand && longBand && noBestBand);

        const std::vector<CaptureTextItem> pastValue = cellsInBand(layout, *shortBand, shortRow.personalBestText);
        ASSERT_EQ(pastValue.size(), 1u);
        const std::vector<QRectF> shortFill =
            rectsFilled(layout, *shortBand, QColor(snapshot.grid.tiers.at(0).barColor));
        ASSERT_EQ(shortFill.size(), 1u);
        const qreal gap = pastValue.front().rect.left() - shortFill.front().right();
        EXPECT_GT(gap, 0.0);
        EXPECT_LT(gap, shortFill.front().height());
        EXPECT_EQ(pastValue.front().color, QColor(shortRow.attainedTierColor));

        const std::vector<CaptureTextItem> insideValue = cellsInBand(layout, *longBand, longRow.personalBestText);
        ASSERT_EQ(insideValue.size(), 1u);
        const std::vector<QRectF> topFill = rectsFilled(layout, *longBand, QColor(lightBar));
        ASSERT_EQ(topFill.size(), 1u);
        EXPECT_LE(insideValue.front().rect.right(), topFill.front().right());
        EXPECT_LT(insideValue.front().color.lightnessF(), 0.5);

        EXPECT_TRUE(cellsInBand(layout, *noBestBand, noBest.personalBestText).empty());

        const auto [darkSnapshot, darkLayout] = layoutWithTopTier(darkBar);
        const std::optional<QRectF> darkBand = bandOfRow(darkSnapshot, darkLayout, 1);
        ASSERT_TRUE(darkBand);
        const std::vector<CaptureTextItem> onDark = cellsInBand(darkLayout, *darkBand, longRow.personalBestText);
        ASSERT_EQ(onDark.size(), 1u);
        EXPECT_GT(onDark.front().color.lightnessF(), 0.5);
    }

    TEST(TableCaptureBars, TheMarkerUsesTheChunkScaleAcrossUnequalChunks) {
        const auto unequalTable = [] {
            TableCaptureSnapshot snapshot = plainTable(2);
            snapshot.grid.tiers.at(0).name = QStringLiteral("Grandmaster Extraordinaire");
            snapshot.grid.rows.at(0).fills = {0.1, 0.0, 0.0};
            snapshot.grid.rows.at(0).recentPosition = 2.5;
            snapshot.grid.rows.at(1).fills = {1.0, 0.0, 0.0};
            return snapshot;
        };

        const TableCaptureSnapshot uncut = unequalTable();
        const CaptureLayout uncutLayout = layoutTableCapture(uncut, false);

        const std::optional<QRectF> markedBand = bandOfRow(uncut, uncutLayout, 0);
        ASSERT_TRUE(markedBand);
        const std::vector<QRectF> ticks = markerTicks(uncut, uncutLayout, *markedBand);
        ASSERT_EQ(ticks.size(), 1u);
        const std::vector<QRectF> chunks = chunkTracks(uncut, uncutLayout, *markedBand);
        ASSERT_EQ(chunks.size(), 3u);
        EXPECT_NE(chunks.at(0).width(), chunks.at(2).width());
        EXPECT_NEAR(ticks.front().center().x(), chunks.at(2).center().x(), 1.0);

        // The marker goes where the value already landed, because the value's place is the layout's to choose.
        const std::optional<QRectF> crossedBand = bandOfRow(uncut, uncutLayout, 1);
        ASSERT_TRUE(crossedBand);
        const std::vector<CaptureTextItem> value =
            cellsInBand(uncutLayout, *crossedBand, uncut.grid.rows.at(1).personalBestText);
        ASSERT_EQ(value.size(), 1u);
        const qreal valueCentre = value.front().rect.center().x();
        const auto chunk = std::ranges::find_if(chunks, [&](const QRectF &track) { return valueCentre < track.right(); });
        ASSERT_NE(chunk, chunks.end());
        const auto chunkIndex = static_cast<qreal>(std::distance(chunks.begin(), chunk));

        TableCaptureSnapshot crossing = unequalTable();
        crossing.grid.rows.at(1).recentPosition = chunkIndex + (valueCentre - chunk->left()) / chunk->width();
        const CaptureLayout crossingLayout = layoutTableCapture(crossing, false);

        const std::optional<QRectF> band = bandOfRow(crossing, crossingLayout, 1);
        ASSERT_TRUE(band);
        const std::vector<CaptureTextItem> crossed =
            cellsInBand(crossingLayout, *band, crossing.grid.rows.at(1).personalBestText);
        ASSERT_EQ(crossed.size(), 1u);
        const std::vector<QRectF> pieces = markerTicks(crossing, crossingLayout, *band);
        ASSERT_EQ(pieces.size(), 2u);

        const qreal textCentre = crossed.front().rect.center().y();
        const qreal digits = digitHalfHeight(crossing, crossed.front());
        EXPECT_LE(pieces.at(0).bottom(), textCentre - digits);
        EXPECT_GE(pieces.at(1).top(), textCentre + digits);
        const std::vector<QRectF> crossedChunks = chunkTracks(crossing, crossingLayout, *band);
        ASSERT_FALSE(crossedChunks.empty());
        EXPECT_LT(pieces.at(0).top(), crossedChunks.front().top());
        EXPECT_GT(pieces.at(1).bottom(), crossedChunks.front().bottom());
    }

    // These meanings live on tooltips in the app, and a hover cannot reach an exported image.
    TEST(TableCaptureBars, BlockersAndAboveTopAreMarkedAndKeyed) {
        TableCaptureSnapshot marked = plainTable(4);
        presentation::ProgressGridScenarioRow &blocker = marked.grid.rows.at(0);
        blocker.blocker = true;
        blocker.blockerColor = marked.grid.tiers.at(1).barColor;
        blocker.blockerText = QStringLiteral("Holds back the next tier");
        presentation::ProgressGridScenarioRow &aboveTop = marked.grid.rows.at(1);
        aboveTop.edgeState = presentation::ProgressGridEdgeState::AboveTop;
        aboveTop.aboveTopText = QStringLiteral("Above the top tier");
        aboveTop.fills = {1.0, 1.0, 1.0};
        marked.grid.rows.at(2).recentPosition = 1.5;

        const CaptureLayout layout = layoutTableCapture(marked, false);

        const std::optional<QRectF> blockerBand = bandOfRow(marked, layout, 0);
        const std::optional<QRectF> aboveTopBand = bandOfRow(marked, layout, 1);
        const std::optional<QRectF> lastBand = bandOfRow(marked, layout, 3);
        ASSERT_TRUE(blockerBand && aboveTopBand && lastBand);

        const std::vector<CaptureGlyphItem> diamonds =
            glyphsIn(layout, *blockerBand, CaptureGlyphItem::Kind::Diamond);
        ASSERT_EQ(diamonds.size(), 1u);
        EXPECT_EQ(diamonds.front().color, QColor(blocker.blockerColor));

        const std::vector<CaptureGlyphItem> chevrons =
            glyphsIn(layout, *aboveTopBand, CaptureGlyphItem::Kind::Chevron);
        ASSERT_EQ(chevrons.size(), 1u);
        const std::vector<QRectF> chunks = chunkTracks(marked, layout, *aboveTopBand);
        ASSERT_EQ(chunks.size(), 3u);
        EXPECT_GT(chevrons.front().rect.center().x(), chunks.back().right());

        const KeyEntries everyKind = keyBelow(marked, layout, *lastBand);
        EXPECT_EQ(everyKind.explanations, 3u);
        EXPECT_EQ(everyKind.diamonds, 1u);
        EXPECT_EQ(everyKind.chevrons, 1u);
        EXPECT_EQ(everyKind.ticks, 1u);

        TableCaptureSnapshot blockerOnly = plainTable(2);
        blockerOnly.grid.rows.at(0).blocker = true;
        blockerOnly.grid.rows.at(0).blockerColor = blockerOnly.grid.tiers.at(1).barColor;
        const CaptureLayout blockerLayout = layoutTableCapture(blockerOnly, false);
        const std::optional<QRectF> blockerLast = bandOfRow(blockerOnly, blockerLayout, 1);
        ASSERT_TRUE(blockerLast);
        const KeyEntries oneKind = keyBelow(blockerOnly, blockerLayout, *blockerLast);
        EXPECT_EQ(oneKind.explanations, 1u);
        EXPECT_EQ(oneKind.diamonds, 1u);
        EXPECT_EQ(oneKind.chevrons, 0u);
        EXPECT_EQ(oneKind.ticks, 0u);

        const TableCaptureSnapshot plain = plainTable(2);
        const CaptureLayout plainLayout = layoutTableCapture(plain, false);
        const std::optional<QRectF> plainLast = bandOfRow(plain, plainLayout, 1);
        ASSERT_TRUE(plainLast);
        const KeyEntries noKind = keyBelow(plain, plainLayout, *plainLast);
        EXPECT_EQ(noKind.explanations, 0u);
    }

    TEST(TableCaptureBars, StatusTextNeverOverflowsItsBar) {
        TableCaptureSnapshot snapshot = plainTable(2, 2);
        snapshot.grid.tiers.at(0).name = QStringLiteral("A");
        snapshot.grid.tiers.at(1).name = QStringLiteral("B");
        presentation::ProgressGridScenarioRow &status = snapshot.grid.rows.at(0);
        status.edgeState = presentation::ProgressGridEdgeState::Unplayed;
        status.statusText = QStringLiteral("This scenario has not been played on this profile yet");

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<QRectF> statusBand = bandOfRow(snapshot, layout, 0);
        const std::optional<QRectF> otherBand = bandOfRow(snapshot, layout, 1);
        const std::optional<CaptureTextItem> heading = textEqualTo(layout, snapshot.grid.headings.scenario);
        ASSERT_TRUE(statusBand && otherBand && heading);

        const std::vector<CaptureTextItem> text = cellsInBand(layout, *statusBand, status.statusText);
        ASSERT_EQ(text.size(), 1u);
        const std::vector<QRectF> chunks = chunkTracks(snapshot, layout, *statusBand);
        ASSERT_EQ(chunks.size(), 2u);
        EXPECT_GE(text.front().rect.left(), chunks.front().left());
        EXPECT_LE(text.front().rect.right(), chunks.back().right());
        EXPECT_GE(text.front().rect.width(), advanceOf(snapshot, text.front()));

        const std::vector<QRectF> otherChunks = chunkTracks(snapshot, layout, *otherBand);
        ASSERT_EQ(otherChunks.size(), chunks.size());
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            EXPECT_DOUBLE_EQ(otherChunks.at(i).left(), chunks.at(i).left());
            EXPECT_DOUBLE_EQ(otherChunks.at(i).right(), chunks.at(i).right());

            const std::vector<CaptureTextItem> tier = cellsBeside(layout, *heading, snapshot.grid.tiers.at(i).name);
            ASSERT_EQ(tier.size(), 1u);
            EXPECT_NEAR(tier.front().rect.center().x(), chunks.at(i).center().x(), 1.0);
        }
    }

    TEST(TableCaptureBars, ATableWithNoTiersKeepsItsValues) {
        TableCaptureSnapshot snapshot = plainTable(2, 0);
        presentation::ProgressGridScenarioRow &status = snapshot.grid.rows.at(1);
        status.edgeState = presentation::ProgressGridEdgeState::Unplayed;
        status.statusText = QStringLiteral("Not played yet");

        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const std::optional<QRectF> valueBand = bandOfRow(snapshot, layout, 0);
        const std::optional<QRectF> statusBand = bandOfRow(snapshot, layout, 1);
        const std::optional<CaptureTextItem> plays = textEqualTo(layout, snapshot.grid.headings.plays);
        const std::optional<CaptureTextItem> next = textEqualTo(layout, snapshot.grid.headings.next);
        ASSERT_TRUE(valueBand && statusBand && plays && next);

        const std::vector<CaptureTextItem> value =
            cellsInBand(layout, *valueBand, snapshot.grid.rows.at(0).personalBestText);
        ASSERT_EQ(value.size(), 1u);
        const std::vector<CaptureTextItem> statusText = cellsInBand(layout, *statusBand, status.statusText);
        ASSERT_EQ(statusText.size(), 1u);

        for (const CaptureTextItem &item : {value.front(), statusText.front()}) {
            EXPECT_GE(item.rect.left(), plays->rect.right());
            EXPECT_LE(item.rect.right(), next->rect.left());
            EXPECT_GE(item.rect.width(), advanceOf(snapshot, item));
        }

        EXPECT_TRUE(chunkTracks(snapshot, layout, *valueBand).empty());
        EXPECT_TRUE(chunkTracks(snapshot, layout, *statusBand).empty());

        const QRectF canvas(QPointF(0.0, 0.0), layout.size);
        for (const CaptureItem &item : layout.items) {
            EXPECT_TRUE(canvas.contains(std::visit([](const auto &held) { return held.rect; }, item)));
        }
    }

    TEST(TableCaptureRegions, TheSummaryChangesHeightButNotTheTable) {
        const TableCaptureSnapshot snapshot = summarisedTable();

        const CaptureLayout withSummary = layoutTableCapture(snapshot, true);
        const CaptureLayout withoutSummary = layoutTableCapture(snapshot, false);

        ASSERT_EQ(countOfText(withSummary, snapshot.benchmarkName), 1);
        EXPECT_EQ(countOfText(withoutSummary, snapshot.benchmarkName), 1);

        const presentation::ProgressGridHeadings &headings = snapshot.grid.headings;
        for (const QString &heading : {headings.category, headings.scenario, headings.pbDate, headings.plays,
                                       headings.next}) {
            EXPECT_GE(countOfText(withSummary, heading), 1) << heading.toStdString();
            EXPECT_EQ(countOfText(withoutSummary, heading), 1) << heading.toStdString();
        }

        const std::optional<qreal> topWith = tableTop(snapshot, withSummary);
        const std::optional<qreal> topWithout = tableTop(snapshot, withoutSummary);
        ASSERT_TRUE(topWith && topWithout);

        const QRectF aboveWith(0.0, 0.0, withSummary.size.width(), *topWith);
        const QRectF aboveWithout(0.0, 0.0, withoutSummary.size.width(), *topWithout);
        for (const QString &text : summaryTexts(*snapshot.summary)) {
            EXPECT_FALSE(cellsInBand(withSummary, aboveWith, text).empty()) << text.toStdString();
            EXPECT_TRUE(cellsInBand(withoutSummary, aboveWithout, text).empty()) << text.toStdString();
        }

        EXPECT_GT(withSummary.size.height(), withoutSummary.size.height());

        const qreal offset = *topWith - *topWithout;
        EXPECT_GT(offset, 0.0);
        std::vector<CaptureItem> moved = itemsFrom(withoutSummary, *topWithout);
        for (CaptureItem &item : moved) {
            std::visit([&](auto &held) { held.rect.translate(0.0, offset); }, item);
        }
        EXPECT_TRUE(sameItems(moved, itemsFrom(withSummary, *topWith)));
    }

    TEST(TableCaptureRegions, TheSummaryShowsEveryValueIncludingTheTooltipOnes) {
        const TableCaptureSnapshot snapshot = summarisedTable();
        const TableCaptureSummary &summary = *snapshot.summary;

        const CaptureLayout layout = layoutTableCapture(snapshot, true);

        const std::optional<qreal> top = tableTop(snapshot, layout);
        ASSERT_TRUE(top);
        const QRectF above(0.0, 0.0, layout.size.width(), *top);

        const auto expectColour = [&](const QString &text, const QString &color) {
            const std::vector<CaptureTextItem> cells = cellsInBand(layout, above, text);
            ASSERT_FALSE(cells.empty()) << text.toStdString();
            const bool anyColoured = std::ranges::any_of(
                cells, [&](const CaptureTextItem &cell) { return cell.color == QColor(color); });
            EXPECT_TRUE(anyColoured) << text.toStdString();
        };
        expectColour(summary.attainedRank, summary.attainedRankColor);
        expectColour(summary.completedRank, summary.completedRankColor);
        expectColour(summary.nextTier, summary.nextTierColor);
        if (::testing::Test::HasFatalFailure()) return;

        for (const QString &text : summaryTexts(summary)) {
            EXPECT_FALSE(cellsInBand(layout, above, text).empty()) << text.toStdString();
        }

        const std::vector<QRectF> chunks = chunkTracks(snapshot, layout, above);
        ASSERT_EQ(chunks.size(), 3u);
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            const std::vector<QRectF> fill =
                rectsFilled(layout, above, QColor(snapshot.grid.tiers.at(i).barColor));
            ASSERT_EQ(fill.size(), 1u);
            EXPECT_NEAR(fill.front().width(), chunks.at(i).width() * summary.averageRankFills.at(i), 1.0);
        }

        const std::vector<QRectF> ticks = rectsFilled(layout, above, snapshot.appearance.text);
        ASSERT_EQ(ticks.size(), 1u);
        EXPECT_NEAR(ticks.front().center().x(), (chunks.at(1).right() + chunks.at(2).left()) / 2.0, 1.0);
    }

    TEST(TableCaptureRegions, AnIncompleteTableStatesItsNoteAndHasNoSummary) {
        TableCaptureSnapshot snapshot = plainTable(3);
        snapshot.completenessNote = QStringLiteral("This benchmark cannot rank you yet");
        const presentation::BenchmarkSummaryCaptions captions;

        const CaptureLayout asked = layoutTableCapture(snapshot, true);
        const CaptureLayout notAsked = layoutTableCapture(snapshot, false);

        ASSERT_EQ(countOfText(asked, snapshot.completenessNote), 1);
        EXPECT_EQ(countOfText(notAsked, snapshot.completenessNote), 1);

        for (const QString &caption : {captions.attainedRank, captions.completedRank, captions.averageRank,
                                       captions.totalPlaytime}) {
            EXPECT_EQ(countOfText(asked, caption), 0) << caption.toStdString();
        }
        EXPECT_EQ(asked.size, notAsked.size);
        EXPECT_TRUE(sameItems(asked.items, notAsked.items));
    }

    TEST(TableCaptureRegions, ANarrowTableStillHoldsItsTitleAndSummary) {
        TableCaptureSnapshot snapshot = plainTable(1, 1);
        const TableCaptureSnapshot measuredAlone = snapshot;
        const std::vector<presentation::ProgressGridTier> summaryTiers = tableFixtureTiers(3);
        snapshot.benchmarkName = longPhrase(QStringLiteral("Titled"));
        snapshot.summary = tableFixtureSummary(summaryTiers);
        snapshot.summary->averageRankFills = {0.4};
        snapshot.summary->nextTierBlockerText = longPhrase(QStringLiteral("Held")).left(160);

        const CaptureLayout layout = layoutTableCapture(snapshot, true);
        const CaptureLayout aloneLayout = layoutTableCapture(measuredAlone, false);

        ASSERT_FALSE(linesSpelling(layout, snapshot.benchmarkName).empty());

        const QRectF canvas(QPointF(0.0, 0.0), layout.size);
        for (const CaptureItem &item : layout.items) {
            EXPECT_TRUE(canvas.contains(std::visit([](const auto &held) { return held.rect; }, item)));
        }

        const presentation::ProgressGridHeadings &headings = snapshot.grid.headings;
        const std::optional<qreal> top = tableTop(snapshot, layout);
        const std::optional<qreal> aloneTop = tableTop(measuredAlone, aloneLayout);
        ASSERT_TRUE(top && aloneTop);
        for (const QString &heading : {headings.category, headings.scenario, headings.pbDate, headings.plays,
                                       headings.next}) {
            const std::vector<CaptureTextItem> cells =
                cellsInBand(layout, QRectF(0.0, *top, layout.size.width(), layout.size.height() - *top), heading);
            const std::optional<CaptureTextItem> alone = textEqualTo(aloneLayout, heading);
            ASSERT_EQ(cells.size(), 1u) << heading.toStdString();
            ASSERT_TRUE(alone) << heading.toStdString();
            EXPECT_DOUBLE_EQ(cells.front().rect.left(), alone->rect.left()) << heading.toStdString();
            EXPECT_DOUBLE_EQ(cells.front().rect.width(), alone->rect.width()) << heading.toStdString();
        }
    }
}
