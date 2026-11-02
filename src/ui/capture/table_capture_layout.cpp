#include "capture/table_capture_layout.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "capture/capture_fonts.h"
#include "capture/capture_text.h"
#include "presentation/benchmarks/benchmark_format.h"

namespace ksv::ui {
    namespace {
        using presentation::ProgressGridContent;
        using presentation::ProgressGridEdgeState;
        using presentation::ProgressGridGroup;
        using presentation::ProgressGridGroupColumn;
        using presentation::ProgressGridScenarioRow;

        // Provisional: the width past which an identity label wraps, and the row height that holds a bar and its lane.
        constexpr qreal kIdentityColumnCap = 280.0;
        constexpr qreal kBaseRowHeight = 42.0;

        constexpr qreal kPageMargin = 16.0;
        constexpr qreal kHeaderHeight = 30.0;
        constexpr qreal kCellPadding = 8.0;
        constexpr qreal kCellVerticalPadding = 4.0;
        constexpr qreal kRuleThickness = 1.0;
        constexpr qreal kBarInset = 8.0;
        constexpr qreal kBarHeight = 16.0;
        constexpr qreal kBarRadius = 2.0;
        constexpr qreal kChunkGap = 2.0;
        constexpr qreal kChunkPadding = 6.0;
        constexpr qreal kTierUnderlineHeight = 4.0;
        constexpr qreal kThresholdInset = 3.0;
        constexpr qreal kTrackAlpha = 0.12;
        constexpr qreal kHatchAlpha = 0.35;
        constexpr qreal kValueGap = 4.0;
        constexpr qreal kAboveTopRoom = 4.0;
        constexpr qreal kStatusInset = 4.0;
        constexpr qreal kMarkerHalfWidth = 2.0;
        constexpr qreal kMarkerRadius = 1.0;
        constexpr qreal kMarkerOverhang = 3.0;
        constexpr qreal kMarkerDigitClearance = 1.0;
        constexpr qreal kChevronBarOverlap = 2.0;
        constexpr qreal kChevronWidth = 7.0;
        constexpr qreal kChevronHeight = 10.0;
        constexpr qreal kChevronLineWidth = 2.0;
        constexpr qreal kDiamondSize = 13.0;
        constexpr qreal kDiamondCentreInset = 4.0;
        constexpr qreal kKeySampleWidth = 13.0;
        constexpr qreal kKeySampleGap = 6.0;
        constexpr qreal kKeyEntryGap = 18.0;
        constexpr qreal kKeyRowGap = 4.0;
        constexpr qreal kMarkerSpace = 16.0;
        constexpr qreal kNextPartGap = 4.0;
        constexpr qreal kChipPadding = 6.0;
        constexpr qreal kChipSpacing = 6.0;
        constexpr qreal kChipExtraHeight = 4.0;
        constexpr qreal kChipFillAlpha = 0.28;
        constexpr qreal kSectionGap = 10.0;
        constexpr qreal kSummaryCellGap = 24.0;
        constexpr qreal kSummaryLineGap = 2.0;
        constexpr qreal kSummaryChunkMinimum = 30.0;
        constexpr qreal kCompletedTickWidth = 2.0;
        constexpr qreal kCompletedTickOverhang = 4.0;

        // A label breaks only between words: an unbreakable word widens its column instead of being cut.
        constexpr QTextOption::WrapMode kWrapMode = QTextOption::WordWrap;

        const Qt::Alignment kLeftAligned = Qt::AlignLeft | Qt::AlignVCenter;
        const Qt::Alignment kRightAligned = Qt::AlignRight | Qt::AlignVCenter;
        const Qt::Alignment kCentred = Qt::AlignHCenter | Qt::AlignVCenter;

        struct Column {
            qreal x = 0.0;
            qreal width = 0.0;

            [[nodiscard]] qreal right() const { return x + width; }
            [[nodiscard]] qreal innerLeft() const { return x + kCellPadding; }
            [[nodiscard]] qreal innerWidth() const { return width - 2.0 * kCellPadding; }
        };

        struct Fonts {
            QFont body;
            QFont bold;
            QFont small;
            QFont tick;
        };

        struct GroupText {
            QStringList lines;
            bool chipBesideName = false;
        };

        // Every coordinate is relative to the table's top-left corner.
        struct TableMetrics {
            Fonts fonts;
            Column category, subcategory, scenario, pbDate, plays, progress, next;
            Column bar;
            std::vector<Column> chunks;
            std::vector<QStringList> nameLines;
            std::vector<GroupText> groupTexts;
            // One more entry than there are rows; the last is the bottom of the last row.
            std::vector<qreal> rowTops;

            [[nodiscard]] qreal width() const { return next.right(); }
            [[nodiscard]] qreal height() const { return rowTops.back(); }
        };

        qreal paddedWidth(const qreal inner) {
            return std::ceil(inner + 2.0 * kCellPadding);
        }

        qreal longestWordWidth(const QFont &font, const QString &text) {
            qreal widest = 0.0;
            for (const QString &word : text.simplified().split(QLatin1Char(' '))) {
                widest = std::max(widest, measuredWidth(font, word));
            }
            return widest;
        }

        qreal cappedWidth(const qreal fullWidth, const qreal unbreakableWidth) {
            return std::max(unbreakableWidth, std::min(fullWidth, kIdentityColumnCap));
        }

        qreal chipWidth(const Fonts &fonts, const QString &text) {
            return measuredWidth(fonts.small, text) + 2.0 * kChipPadding;
        }

        qreal chipHeight(const Fonts &fonts) {
            return lineHeight(fonts.small) + kChipExtraHeight;
        }

        bool isStatusRow(const ProgressGridScenarioRow &row) {
            return row.edgeState == ProgressGridEdgeState::Unplayed || row.edgeState == ProgressGridEdgeState::Unmatched;
        }

        bool showsStatus(const ProgressGridScenarioRow &row) {
            return isStatusRow(row) && !row.statusText.isEmpty();
        }

        bool showsValue(const ProgressGridScenarioRow &row) {
            return row.hasPersonalBest && !row.personalBestText.isEmpty() && !isStatusRow(row);
        }

        bool isMissing(const ProgressGridScenarioRow &row, const std::size_t chunk) {
            return chunk < row.missingChunks.size() && row.missingChunks[chunk];
        }

        // Unlike the live bar this ignores the value and the marker: the lane is there so neither can hide a threshold.
        bool showsThreshold(const ProgressGridScenarioRow &row, const std::size_t chunk) {
            return chunk < row.thresholdTexts.size() && !row.thresholdTexts[chunk].isEmpty() && !isMissing(row, chunk) &&
                   !isStatusRow(row);
        }

        qreal aboveTopRoom(const ProgressGridScenarioRow &row) {
            return row.edgeState == ProgressGridEdgeState::AboveTop ? kAboveTopRoom : 0.0;
        }

        qreal laneHeight(const Fonts &fonts) {
            return lineHeight(fonts.tick);
        }

        qreal barHeight(const Fonts &fonts) {
            return std::max(kBarHeight, lineHeight(fonts.small));
        }

        qreal headerHeight(const Fonts &fonts) {
            return std::max(kHeaderHeight, lineHeight(fonts.bold) + 2.0 * kCellVerticalPadding);
        }

        qreal barBlockHeight(const Fonts &fonts) {
            return barHeight(fonts) + kMarkerOverhang + laneHeight(fonts);
        }

        qreal baseRowHeight(const Fonts &fonts) {
            return std::max(kBaseRowHeight, barBlockHeight(fonts) + 2.0 * kMarkerOverhang);
        }

        // The widest a bar has to be for every row's value to sit past its fill or inside it, wherever the fill ends:
        // at twice the value's room, a fill too short to hold the value leaves room for it past the fill's end.
        qreal requiredBarWidth(const ProgressGridContent &grid, const Fonts &fonts) {
            qreal required = 0.0;
            for (const ProgressGridScenarioRow &row : grid.rows) {
                if (showsValue(row)) {
                    const qreal room = measuredWidth(fonts.small, row.personalBestText) + kValueGap + aboveTopRoom(row);
                    required = std::max(required, 2.0 * room);
                }
                if (showsStatus(row)) {
                    required = std::max(required, measuredWidth(fonts.small, row.statusText) + 2.0 * kStatusInset);
                }
            }
            return required;
        }

        std::vector<qreal> chunkWidths(const ProgressGridContent &grid, const Fonts &fonts) {
            std::vector<qreal> widths;
            for (std::size_t i = 0; i < grid.tiers.size(); ++i) {
                qreal width = measuredWidth(fonts.bold, grid.tiers[i].name) + 2.0 * kChunkPadding;
                for (const ProgressGridScenarioRow &row : grid.rows) {
                    if (!showsThreshold(row, i)) continue;
                    width = std::max(width, measuredWidth(fonts.tick, row.thresholdTexts[i]) + 2.0 * kThresholdInset);
                }
                widths.push_back(width);
            }
            if (widths.empty()) return widths;

            const auto count = static_cast<qreal>(widths.size());
            const qreal barWidth = std::reduce(widths.begin(), widths.end(), 0.0) + (count - 1.0) * kChunkGap;
            const qreal share = std::ceil(std::max(0.0, requiredBarWidth(grid, fonts) - barWidth) / count);
            for (qreal &width : widths) width += share;
            return widths;
        }

        qreal groupInnerWidth(const ProgressGridGroup &group, const Fonts &fonts) {
            const qreal name = measuredWidth(fonts.bold, group.name);
            const qreal word = longestWordWidth(fonts.bold, group.name);
            if (group.rankText.isEmpty()) return cappedWidth(name, word);
            const qreal chip = chipWidth(fonts, group.rankText);
            return cappedWidth(name + kChipSpacing + chip, std::max(word, chip));
        }

        bool spansBothColumns(const ProgressGridContent &grid, const ProgressGridGroup &group) {
            return grid.hasSubcategories && group.spansSubcategory;
        }

        void measureColumns(const ProgressGridContent &grid, TableMetrics &metrics) {
            const Fonts &fonts = metrics.fonts;

            qreal categoryInner = measuredWidth(fonts.bold, grid.headings.category);
            qreal subcategoryInner = grid.hasSubcategories ? measuredWidth(fonts.bold, grid.headings.subcategory) : 0.0;
            qreal spanningInner = 0.0;
            for (const ProgressGridGroup &group : grid.groups) {
                const qreal inner = groupInnerWidth(group, fonts);
                if (spansBothColumns(grid, group)) {
                    spanningInner = std::max(spanningInner, inner);
                } else if (group.column == ProgressGridGroupColumn::Subcategory) {
                    subcategoryInner = std::max(subcategoryInner, inner);
                } else {
                    categoryInner = std::max(categoryInner, inner);
                }
            }

            qreal nameWidth = 0.0;
            qreal nameWord = 0.0;
            qreal pbDateInner = measuredWidth(fonts.bold, grid.headings.pbDate);
            qreal playsInner = measuredWidth(fonts.bold, grid.headings.plays);
            qreal nextInner = measuredWidth(fonts.bold, grid.headings.next);
            for (const ProgressGridScenarioRow &row : grid.rows) {
                nameWidth = std::max(nameWidth, measuredWidth(fonts.body, row.name));
                nameWord = std::max(nameWord, longestWordWidth(fonts.body, row.name));
                pbDateInner = std::max(pbDateInner, measuredWidth(fonts.body, row.personalBestDateText));
                playsInner = std::max(playsInner, measuredWidth(fonts.body, row.playCountText));
                const qreal tierPart = row.nextTierText.isEmpty()
                                           ? 0.0
                                           : measuredWidth(fonts.body, row.nextTierText) + kNextPartGap;
                nextInner = std::max(nextInner, tierPart + measuredWidth(fonts.body, row.nextThresholdText));
            }
            const qreal scenarioInner = std::max(measuredWidth(fonts.bold, grid.headings.scenario),
                                                 kMarkerSpace + cappedWidth(nameWidth, nameWord));

            const std::vector<qreal> chunkSizes = chunkWidths(grid, fonts);
            const qreal barWidth = chunkSizes.empty()
                                       ? requiredBarWidth(grid, fonts)
                                       : std::reduce(chunkSizes.begin(), chunkSizes.end(), 0.0) +
                                         static_cast<qreal>(chunkSizes.size() - 1) * kChunkGap;

            qreal x = 0.0;
            const auto place = [&x](Column &column, const qreal width) {
                column = {x, width};
                x += width;
            };
            const qreal subcategoryWidth = grid.hasSubcategories ? paddedWidth(subcategoryInner) : 0.0;
            const qreal categoryWidth = std::max(paddedWidth(categoryInner),
                                                 spanningInner > 0.0
                                                     ? paddedWidth(spanningInner) - subcategoryWidth
                                                     : 0.0);
            place(metrics.category, categoryWidth);
            place(metrics.subcategory, subcategoryWidth);
            place(metrics.scenario, paddedWidth(scenarioInner));
            place(metrics.pbDate, paddedWidth(pbDateInner));
            place(metrics.plays, paddedWidth(playsInner));
            place(metrics.progress, barWidth + 2.0 * kBarInset);
            place(metrics.next, paddedWidth(nextInner));

            metrics.bar = {metrics.progress.x + kBarInset, barWidth};
            qreal chunkLeft = metrics.bar.x;
            for (const qreal width : chunkSizes) {
                metrics.chunks.push_back({chunkLeft, width});
                chunkLeft += width + kChunkGap;
            }
        }

        Column groupColumn(const ProgressGridContent &grid, const ProgressGridGroup &group,
                           const TableMetrics &metrics) {
            if (spansBothColumns(grid, group)) return {metrics.category.x, metrics.category.width + metrics.subcategory.width};
            return group.column == ProgressGridGroupColumn::Subcategory ? metrics.subcategory : metrics.category;
        }

        qreal groupContentHeight(const ProgressGridGroup &group, const GroupText &text, const Fonts &fonts) {
            const qreal lines = static_cast<qreal>(text.lines.size()) * lineHeight(fonts.bold);
            const bool chipBelowName = !group.rankText.isEmpty() && !text.chipBesideName;
            return lines + (chipBelowName ? chipHeight(fonts) : 0.0);
        }

        // A group's cell can outgrow its rows. Only ever raising rows means a group settled here stays settled, so
        // nested category and subcategory groups need no ordering.
        void measureRows(const ProgressGridContent &grid, TableMetrics &metrics) {
            const Fonts &fonts = metrics.fonts;
            const qreal nameWidth = metrics.scenario.innerWidth() - kMarkerSpace;

            std::vector<qreal> heights;
            for (const ProgressGridScenarioRow &row : grid.rows) {
                QStringList lines = wrappedLines(row.name, fonts.body, nameWidth, kWrapMode);
                const qreal text = static_cast<qreal>(lines.size()) * lineHeight(fonts.body);
                heights.push_back(std::max(baseRowHeight(fonts), text + 2.0 * kCellVerticalPadding));
                metrics.nameLines.push_back(std::move(lines));
            }

            for (const ProgressGridGroup &group : grid.groups) {
                const qreal available = groupColumn(grid, group, metrics).innerWidth();
                GroupText text{wrappedLines(group.name, fonts.bold, available, kWrapMode), false};
                text.chipBesideName = !group.rankText.isEmpty() && text.lines.size() == 1 &&
                                      measuredWidth(fonts.bold, group.name) + kChipSpacing +
                                      chipWidth(fonts, group.rankText) <= available;

                const auto first = heights.begin() + group.start;
                const auto last = first + group.span;
                const qreal height = std::reduce(first, last, 0.0);
                const qreal needed = groupContentHeight(group, text, fonts) + 2.0 * kCellVerticalPadding;
                if (needed > height) *(last - 1) += needed - height;
                metrics.groupTexts.push_back(std::move(text));
            }

            metrics.rowTops = {headerHeight(fonts)};
            for (const qreal height : heights) metrics.rowTops.push_back(metrics.rowTops.back() + height);
        }

        TableMetrics measureTable(const TableCaptureSnapshot &snapshot) {
            const CaptureFontDescription &font = snapshot.appearance.font;
            TableMetrics metrics;
            metrics.fonts = {captureFont(font, CaptureTextRole::Body), captureFont(font, CaptureTextRole::BodyBold),
                             captureFont(font, CaptureTextRole::Small), captureFont(font, CaptureTextRole::Tick)};
            measureColumns(snapshot.grid, metrics);
            measureRows(snapshot.grid, metrics);
            return metrics;
        }

        void appendRect(std::vector<CaptureItem> &items, const QRectF &rect, const QColor &fill,
                        const QColor &border = QColor(), const qreal radius = 0.0) {
            items.emplace_back(CaptureRectItem{rect, fill, border, radius});
        }

        void appendText(std::vector<CaptureItem> &items, const QRectF &rect, const QString &text,
                        const CaptureTextRole role, const QColor &color, const Qt::Alignment alignment) {
            items.emplace_back(CaptureTextItem{rect, text, role, color, alignment, false});
        }

        void appendHeaderCell(std::vector<CaptureItem> &items, const CaptureAppearance &appearance,
                              const qreal headerHeight, const Column &column, const QString &text,
                              const Qt::Alignment alignment) {
            if (column.width <= 0.0) return;
            appendRect(items, QRectF(column.x, 0.0, column.width, headerHeight), appearance.button, appearance.mid);
            appendText(items, QRectF(column.innerLeft(), 0.0, column.innerWidth(), headerHeight), text,
                       CaptureTextRole::BodyBold, appearance.text, alignment);
        }

        void appendTierHeaders(std::vector<CaptureItem> &items, const ProgressGridContent &grid,
                               const CaptureAppearance &appearance, const TableMetrics &metrics) {
            for (std::size_t i = 0; i < grid.tiers.size(); ++i) {
                const presentation::ProgressGridTier &tier = grid.tiers[i];
                const Column &chunk = metrics.chunks[i];
                // The outer cells also cover the bar's inset, so it does not read as a column of its own.
                const qreal left = i == 0 ? metrics.progress.x : chunk.x - kChunkGap / 2.0;
                const qreal right = i + 1 == grid.tiers.size() ? metrics.progress.right()
                                                               : chunk.right() + kChunkGap / 2.0;

                const QColor barColor(tier.barColor);
                const qreal height = headerHeight(metrics.fonts);
                appendRect(items, QRectF(left, 0.0, right - left, height), appearance.button, appearance.mid);
                appendRect(items, QRectF(left, height - kTierUnderlineHeight, right - left, kTierUnderlineHeight),
                           barColor);
                appendText(items, QRectF(chunk.x, 0.0, chunk.width, height), tier.name,
                           CaptureTextRole::BodyBold,
                           presentation::readableColorOn(barColor, appearance.button, appearance.buttonText), kCentred);
            }
        }

        void appendHeader(std::vector<CaptureItem> &items, const TableCaptureSnapshot &snapshot,
                          const TableMetrics &metrics) {
            const CaptureAppearance &appearance = snapshot.appearance;
            const presentation::ProgressGridHeadings &headings = snapshot.grid.headings;
            const qreal height = headerHeight(metrics.fonts);
            appendHeaderCell(items, appearance, height, metrics.category, headings.category, kLeftAligned);
            appendHeaderCell(items, appearance, height, metrics.subcategory, headings.subcategory, kLeftAligned);
            appendHeaderCell(items, appearance, height, metrics.scenario, headings.scenario, kLeftAligned);
            appendHeaderCell(items, appearance, height, metrics.pbDate, headings.pbDate, kRightAligned);
            appendHeaderCell(items, appearance, height, metrics.plays, headings.plays, kRightAligned);
            appendRect(items, QRectF(metrics.progress.x, 0.0, metrics.progress.width, height), appearance.button,
                       appearance.mid);
            appendHeaderCell(items, appearance, height, metrics.next, headings.next, kRightAligned);
            appendTierHeaders(items, snapshot.grid, appearance, metrics);
        }

        void appendValueCell(std::vector<CaptureItem> &items, const CaptureAppearance &appearance,
                             const Column &column, const QRectF &row, const QString &text, const bool hasValue) {
            appendText(items, QRectF(column.innerLeft(), row.top(), column.innerWidth(), row.height()), text,
                       CaptureTextRole::Body, hasValue ? appearance.text : appearance.placeholderText, kRightAligned);
        }

        void appendNextCell(std::vector<CaptureItem> &items, const ProgressGridScenarioRow &row,
                            const CaptureAppearance &appearance, const TableMetrics &metrics, const QRectF &band) {
            const qreal right = metrics.next.right() - kCellPadding;
            const qreal thresholdWidth = measuredWidth(metrics.fonts.body, row.nextThresholdText);
            const bool hasTier = !row.nextTierText.isEmpty();
            appendText(items, QRectF(right - thresholdWidth, band.top(), thresholdWidth, band.height()),
                       row.nextThresholdText, CaptureTextRole::Body,
                       hasTier ? appearance.text : appearance.placeholderText, kRightAligned);
            if (!hasTier) return;

            const qreal tierWidth = measuredWidth(metrics.fonts.body, row.nextTierText);
            appendText(items,
                       QRectF(right - thresholdWidth - kNextPartGap - tierWidth, band.top(), tierWidth, band.height()),
                       row.nextTierText, CaptureTextRole::Body,
                       row.nextTierColor.isEmpty() ? appearance.text : QColor(row.nextTierColor), kRightAligned);
        }

        // ----- tier bar: ports of TierBar.qml's fill, inside-or-past and marker rules -----

        qreal fillFraction(const std::vector<double> &fills, const std::size_t chunk) {
            const double value = chunk < fills.size() ? fills[chunk] : 0.0;
            return std::isnan(value) ? 0.0 : std::clamp(value, 0.0, 1.0);
        }

        qreal fillWidth(const std::vector<double> &fills, const qreal chunkWidth, const std::size_t index) {
            return std::round(chunkWidth * fillFraction(fills, index));
        }

        qreal fillEnd(const ProgressGridScenarioRow &row, const TableMetrics &metrics) {
            for (std::size_t i = metrics.chunks.size(); i-- > 0;) {
                const qreal width = fillWidth(row.fills, metrics.chunks[i].width, i);
                if (width > 0.0) return metrics.chunks[i].x + width;
            }
            return metrics.bar.x;
        }

        struct ValuePlacement {
            QRectF rect;
            bool insideFill = false;
        };

        ValuePlacement placeValue(const ProgressGridScenarioRow &row, const TableMetrics &metrics, const QRectF &bar) {
            const qreal width = measuredWidth(metrics.fonts.small, row.personalBestText);
            const qreal end = fillEnd(row, metrics);
            const qreal room = aboveTopRoom(row);
            const bool inside = end + kValueGap + width > bar.right() - room;
            const qreal left = inside ? std::max(bar.left(), end - kValueGap - room - width) : end + kValueGap;
            return {QRectF(left, bar.top(), width, bar.height()), inside};
        }

        QColor tierBarColor(const presentation::ProgressGridTier &tier, const CaptureAppearance &appearance) {
            return tier.barColor.isEmpty() ? appearance.highlight : QColor(tier.barColor);
        }

        QColor valueColor(const ProgressGridScenarioRow &row, const ProgressGridContent &grid,
                          const CaptureAppearance &appearance, const bool insideFill) {
            const QColor attained = row.attainedTierColor.isEmpty() ? appearance.placeholderText
                                                                    : QColor(row.attainedTierColor);
            if (!insideFill) return attained;
            for (std::size_t i = grid.tiers.size(); i-- > 0;) {
                if (fillFraction(row.fills, i) <= 0.0) continue;
                return presentation::isLightFill(tierBarColor(grid.tiers[i], appearance))
                           ? QColor::fromRgbF(0.1f, 0.1f, 0.1f, 0.9f)
                           : QColor::fromRgbF(1.0f, 1.0f, 1.0f, 0.95f);
            }
            return attained;
        }

        qreal scaleX(const double position, const TableMetrics &metrics) {
            const auto count = static_cast<double>(metrics.chunks.size());
            const double clamped = std::clamp(position, 0.0, count);
            const auto index = static_cast<std::size_t>(std::min(count - 1.0, std::floor(clamped)));
            const Column &chunk = metrics.chunks[index];
            return chunk.x + (clamped - static_cast<double>(index)) * chunk.width;
        }

        void appendMarkerTick(std::vector<CaptureItem> &items, const CaptureAppearance &appearance, const qreal x,
                              const qreal top, const qreal height) {
            appendRect(items, QRectF(x - kMarkerHalfWidth, top, 2.0 * kMarkerHalfWidth, height), appearance.text,
                       appearance.base, kMarkerRadius);
        }

        // The tick overhangs the bar so its ends always show. Across the value it keeps only those ends, each reaching
        // into the bar just short of the digits, so the value stays legible.
        void appendMarker(std::vector<CaptureItem> &items, const double position, const QRectF &bar,
                          const QRectF *value, const CaptureAppearance &appearance, const TableMetrics &metrics) {
            const qreal x = scaleX(position, metrics);
            const qreal top = bar.top() - kMarkerOverhang;
            const qreal fullHeight = bar.height() + 2.0 * kMarkerOverhang;
            const bool crossesValue = value && x + kMarkerHalfWidth + 1.0 > value->left() &&
                                      x - kMarkerHalfWidth - 1.0 < value->right();
            if (!crossesValue) {
                appendMarkerTick(items, appearance, x, top, fullHeight);
                return;
            }
            const qreal digitMargin = (bar.height() - QFontMetricsF(metrics.fonts.small).capHeight()) / 2.0;
            const qreal endLength = kMarkerOverhang + std::max(0.0, digitMargin - kMarkerDigitClearance);
            appendMarkerTick(items, appearance, x, top, endLength);
            appendMarkerTick(items, appearance, x, top + fullHeight - endLength, endLength);
        }

        void appendTrackAndFill(std::vector<CaptureItem> &items, const QRectF &chunk, const qreal fill,
                                const QColor &color, const CaptureAppearance &appearance) {
            QColor track = appearance.text;
            track.setAlphaF(kTrackAlpha);
            appendRect(items, chunk, track, QColor(), kBarRadius);
            if (fill > 0.0) {
                appendRect(items, QRectF(chunk.left(), chunk.top(), fill, chunk.height()), color, QColor(), kBarRadius);
            }
        }

        void appendChunks(std::vector<CaptureItem> &items, const ProgressGridScenarioRow &row,
                          const ProgressGridContent &grid, const CaptureAppearance &appearance,
                          const TableMetrics &metrics, const QRectF &bar) {
            QColor stripe = appearance.text;
            stripe.setAlphaF(kHatchAlpha);

            const qreal laneTop = bar.bottom() + kMarkerOverhang;
            const qreal laneSize = laneHeight(metrics.fonts);
            for (std::size_t i = 0; i < metrics.chunks.size(); ++i) {
                const Column &chunk = metrics.chunks[i];
                const QRectF chunkRect(chunk.x, bar.top(), chunk.width, bar.height());
                appendTrackAndFill(items, chunkRect, fillWidth(row.fills, chunk.width, i),
                                   tierBarColor(grid.tiers[i], appearance), appearance);
                if (isMissing(row, i)) items.emplace_back(CaptureHatchItem{chunkRect, stripe});
                if (showsThreshold(row, i)) {
                    appendText(items,
                               QRectF(chunk.x + kThresholdInset, laneTop, chunk.width - 2.0 * kThresholdInset, laneSize),
                               row.thresholdTexts[i], CaptureTextRole::Tick, appearance.placeholderText, kRightAligned);
                }
            }
        }

        void appendAboveTopMark(std::vector<CaptureItem> &items, const QRectF &bar, const QColor &color,
                                const CaptureAppearance &appearance) {
            const QRectF rect(bar.right() - kChevronBarOverlap, bar.center().y() - kChevronHeight / 2.0, kChevronWidth,
                              kChevronHeight);
            items.emplace_back(CaptureGlyphItem{CaptureGlyphItem::Kind::Chevron, rect, color, appearance.base,
                                                kChevronLineWidth});
        }

        void appendBar(std::vector<CaptureItem> &items, const ProgressGridScenarioRow &row,
                       const ProgressGridContent &grid, const CaptureAppearance &appearance,
                       const TableMetrics &metrics, const QRectF &band) {
            const qreal top = band.top() + (band.height() - barBlockHeight(metrics.fonts)) / 2.0;
            const QRectF bar(metrics.bar.x, top, metrics.bar.width, barHeight(metrics.fonts));
            appendChunks(items, row, grid, appearance, metrics, bar);

            std::optional<QRectF> valueRect;
            if (showsValue(row)) {
                const auto [rect, inside] = placeValue(row, metrics, bar);
                appendText(items, rect, row.personalBestText, CaptureTextRole::Small,
                           valueColor(row, grid, appearance, inside), kLeftAligned);
                valueRect = rect;
            }

            const bool hasChunks = !metrics.chunks.empty();
            if (hasChunks && row.recentPosition && *row.recentPosition >= 0.0) {
                appendMarker(items, *row.recentPosition, bar, valueRect ? &*valueRect : nullptr, appearance, metrics);
            }

            const bool unmatched = row.edgeState == ProgressGridEdgeState::Unmatched;
            if (unmatched) appendRect(items, bar, appearance.base, appearance.attention, kBarRadius);
            if (showsStatus(row)) {
                const qreal width = measuredWidth(metrics.fonts.small, row.statusText);
                appendText(items, QRectF(bar.center().x() - width / 2.0, bar.top(), width, bar.height()),
                           row.statusText, CaptureTextRole::Small,
                           unmatched ? appearance.attention : appearance.placeholderText, kCentred);
            }

            if (hasChunks && row.edgeState == ProgressGridEdgeState::AboveTop) {
                appendAboveTopMark(items, bar, tierBarColor(grid.tiers.back(), appearance), appearance);
            }
        }

        void appendBlocker(std::vector<CaptureItem> &items, const ProgressGridScenarioRow &row,
                           const CaptureAppearance &appearance, const TableMetrics &metrics, const QRectF &band) {
            const qreal centreX = metrics.scenario.innerLeft() - kDiamondCentreInset + kMarkerSpace / 2.0;
            const QRectF rect(centreX - kDiamondSize / 2.0, band.center().y() - kDiamondSize / 2.0, kDiamondSize,
                              kDiamondSize);
            items.emplace_back(CaptureGlyphItem{CaptureGlyphItem::Kind::Diamond, rect,
                                                row.blockerColor.isEmpty() ? appearance.highlight
                                                                           : QColor(row.blockerColor),
                                                QColor(), 0.0});
        }

        void appendRow(std::vector<CaptureItem> &items, const ProgressGridScenarioRow &row,
                       const ProgressGridContent &grid, const QStringList &nameLines, const QRectF &band,
                       const CaptureAppearance &appearance, const TableMetrics &metrics) {
            appendRect(items, band, appearance.base);
            appendRect(items, QRectF(band.left(), band.bottom() - kRuleThickness, band.width(), kRuleThickness),
                       appearance.mid);
            for (const Column *column : {&metrics.category, &metrics.subcategory, &metrics.scenario, &metrics.pbDate,
                                         &metrics.plays, &metrics.progress, &metrics.next}) {
                if (column->width <= 0.0) continue;
                appendRect(items, QRectF(column->right() - kRuleThickness, band.top(), kRuleThickness, band.height()),
                           appearance.mid);
            }

            const qreal lineSize = lineHeight(metrics.fonts.body);
            const qreal nameLeft = metrics.scenario.innerLeft() + kMarkerSpace;
            const qreal nameWidth = metrics.scenario.innerWidth() - kMarkerSpace;
            qreal lineTop = band.top() + (band.height() - static_cast<qreal>(nameLines.size()) * lineSize) / 2.0;
            for (const QString &line : nameLines) {
                appendText(items, QRectF(nameLeft, lineTop, nameWidth, lineSize), line, CaptureTextRole::Body,
                           appearance.text, kLeftAligned);
                lineTop += lineSize;
            }

            appendValueCell(items, appearance, metrics.pbDate, band, row.personalBestDateText, row.hasPersonalBestDate);
            appendValueCell(items, appearance, metrics.plays, band, row.playCountText, row.hasPlayCount);
            appendNextCell(items, row, appearance, metrics, band);
            if (row.blocker) appendBlocker(items, row, appearance, metrics, band);
            appendBar(items, row, grid, appearance, metrics, band);
        }

        void appendChip(std::vector<CaptureItem> &items, const ProgressGridGroup &group,
                        const CaptureAppearance &appearance, const Fonts &fonts, const QPointF &topLeft) {
            const qreal height = chipHeight(fonts);
            const QRectF rect(topLeft.x(), topLeft.y(), chipWidth(fonts, group.rankText), height);

            const bool hasTier = !group.rankBarColor.isEmpty();
            QColor fill = hasTier ? QColor(group.rankBarColor) : appearance.base;
            if (hasTier) fill.setAlphaF(kChipFillAlpha);
            appendRect(items, rect, fill, hasTier ? QColor(group.rankBarColor) : appearance.mid, height / 2.0);
            appendText(items, rect.adjusted(kChipPadding, 0.0, -kChipPadding, 0.0), group.rankText,
                       CaptureTextRole::Small,
                       group.rankColor.isEmpty() ? appearance.placeholderText : QColor(group.rankColor), kCentred);
        }

        void appendGroup(std::vector<CaptureItem> &items, const ProgressGridContent &grid,
                         const ProgressGridGroup &group, const GroupText &text, const CaptureAppearance &appearance,
                         const TableMetrics &metrics) {
            const Column column = groupColumn(grid, group, metrics);
            const qreal top = metrics.rowTops[group.start];
            const QRectF cell(column.x, top, column.width, metrics.rowTops[group.start + group.span] - top);

            appendRect(items, cell, appearance.base);
            if (!group.color.isEmpty()) {
                QColor tint(group.color);
                tint.setAlphaF(appearance.groupTintAlpha);
                appendRect(items, cell, tint);
            }
            appendRect(items, QRectF(cell.left(), cell.bottom() - kRuleThickness, cell.width(), kRuleThickness),
                       appearance.mid);
            appendRect(items, QRectF(cell.right() - kRuleThickness, cell.top(), kRuleThickness, cell.height()),
                       appearance.mid);

            // As live, the content sits in the first row, so a short name stays beside its first scenario.
            const Fonts &fonts = metrics.fonts;
            const qreal firstRowHeight = metrics.rowTops[group.start + 1] - top;
            const qreal contentHeight = groupContentHeight(group, text, fonts);
            const qreal lineSize = lineHeight(fonts.bold);
            qreal y = top + std::max(kCellVerticalPadding, (firstRowHeight - contentHeight) / 2.0);
            for (const QString &line : text.lines) {
                appendText(items, QRectF(column.innerLeft(), y, column.innerWidth(), lineSize), line,
                           CaptureTextRole::BodyBold, appearance.text, kLeftAligned);
                y += lineSize;
            }

            if (group.rankText.isEmpty()) return;
            if (text.chipBesideName) {
                const qreal chipLeft = column.innerLeft() + measuredWidth(fonts.bold, group.name) + kChipSpacing;
                appendChip(items, group, appearance, fonts, QPointF(chipLeft, y - (lineSize + chipHeight(fonts)) / 2.0));
            } else {
                appendChip(items, group, appearance, fonts, QPointF(column.innerLeft(), y));
            }
        }

        // A self-contained block of the page, in its own coordinates; empty when it has nothing to show.
        struct Region {
            std::vector<CaptureItem> items;
            QSizeF size;
        };

        Region tableRegion(const TableCaptureSnapshot &snapshot, const TableMetrics &metrics) {
            const ProgressGridContent &grid = snapshot.grid;
            Region region;
            appendHeader(region.items, snapshot, metrics);
            for (std::size_t i = 0; i < grid.rows.size(); ++i) {
                const QRectF band(0.0, metrics.rowTops[i], metrics.width(), metrics.rowTops[i + 1] - metrics.rowTops[i]);
                appendRow(region.items, grid.rows[i], grid, metrics.nameLines[i], band, snapshot.appearance, metrics);
            }
            for (std::size_t i = 0; i < grid.groups.size(); ++i) {
                appendGroup(region.items, grid, grid.groups[i], metrics.groupTexts[i], snapshot.appearance, metrics);
            }
            region.size = QSizeF(metrics.width(), metrics.height());
            return region;
        }

        // ----- passive key: what the bar's marks mean, which the app says only on tooltips -----

        enum class KeyMark { RecentAverage, Blocker, AboveTop };

        struct KeyEntry {
            KeyMark mark;
            QString text;
        };

        std::vector<KeyEntry> keyEntries(const ProgressGridContent &grid) {
            const auto anyRow = [&grid](const auto &predicate) { return std::ranges::any_of(grid.rows, predicate); };
            const bool hasChunks = !grid.tiers.empty();

            std::vector<KeyEntry> entries;
            if (hasChunks && anyRow([](const ProgressGridScenarioRow &row) {
                    return row.recentPosition && *row.recentPosition >= 0.0;
                })) {
                entries.push_back({KeyMark::RecentAverage, QObject::tr("Recent average")});
            }
            if (anyRow([](const ProgressGridScenarioRow &row) { return row.blocker; })) {
                entries.push_back({KeyMark::Blocker, QObject::tr("Holding back the next tier")});
            }
            if (hasChunks && anyRow([](const ProgressGridScenarioRow &row) {
                    return row.edgeState == ProgressGridEdgeState::AboveTop;
                })) {
                entries.push_back({KeyMark::AboveTop, QObject::tr("Above the top tier")});
            }
            return entries;
        }

        void appendKeySample(std::vector<CaptureItem> &items, const KeyMark mark, const QRectF &slot,
                             const TableCaptureSnapshot &snapshot) {
            const CaptureAppearance &appearance = snapshot.appearance;
            const QPointF centre = slot.center();
            switch (mark) {
                case KeyMark::RecentAverage:
                    appendMarkerTick(items, appearance, centre.x(), slot.top(), slot.height());
                    break;
                case KeyMark::Blocker:
                    items.emplace_back(CaptureGlyphItem{CaptureGlyphItem::Kind::Diamond, slot, appearance.highlight,
                                                        QColor(), 0.0});
                    break;
                case KeyMark::AboveTop: {
                    const QRectF rect(centre.x() - kChevronWidth / 2.0, centre.y() - kChevronHeight / 2.0,
                                      kChevronWidth, kChevronHeight);
                    items.emplace_back(CaptureGlyphItem{CaptureGlyphItem::Kind::Chevron, rect,
                                                        tierBarColor(snapshot.grid.tiers.back(), appearance),
                                                        appearance.base, kChevronLineWidth});
                    break;
                }
            }
        }

        // Entries flow along the table's width, but an entry is never wrapped: one wider than that widens the key.
        Region keyRegion(const TableCaptureSnapshot &snapshot, const TableMetrics &metrics, const qreal width) {
            const std::vector<KeyEntry> entries = keyEntries(snapshot.grid);
            Region region;
            if (entries.empty()) return region;

            const qreal rowHeight = std::max(lineHeight(metrics.fonts.small), kDiamondSize);
            qreal x = 0.0;
            qreal y = 0.0;
            qreal widest = 0.0;
            for (const KeyEntry &entry : entries) {
                const qreal textWidth = measuredWidth(metrics.fonts.small, entry.text);
                const qreal entryWidth = kKeySampleWidth + kKeySampleGap + textWidth;
                if (x > 0.0 && x + entryWidth > width) {
                    x = 0.0;
                    y += rowHeight + kKeyRowGap;
                }
                appendKeySample(region.items, entry.mark, QRectF(x, y, kKeySampleWidth, rowHeight), snapshot);
                appendText(region.items, QRectF(x + kKeySampleWidth + kKeySampleGap, y, textWidth, rowHeight), entry.text,
                           CaptureTextRole::Small, snapshot.appearance.text, kLeftAligned);
                widest = std::max(widest, x + entryWidth);
                x += entryWidth + kKeyEntryGap;
            }
            region.size = QSizeF(widest, y + rowHeight);
            return region;
        }

        // ----- title, note and summary -----

        // A word wider than `width` keeps its line whole, so the region can come out wider than it was asked to be.
        Region wrappedTextRegion(const QString &text, const CaptureTextRole role, const QColor &color,
                                 const CaptureFontDescription &description, const qreal width) {
            const QFont font = captureFont(description, role);
            const qreal height = lineHeight(font);
            Region region;
            qreal y = 0.0;
            qreal widest = 0.0;
            for (const QString &line : wrappedLines(text, font, width, kWrapMode)) {
                const qreal lineWidth = std::max(width, measuredWidth(font, line));
                appendText(region.items, QRectF(0.0, y, lineWidth, height), line, role, color, kLeftAligned);
                widest = std::max(widest, lineWidth);
                y += height;
            }
            region.size = QSizeF(widest, y);
            return region;
        }

        void appendAverageRankBar(std::vector<CaptureItem> &items, const TableCaptureSummary &summary,
                                  const ProgressGridContent &grid, const CaptureAppearance &appearance,
                                  const QRectF &bar) {
            const auto count = static_cast<qreal>(grid.tiers.size());
            if (grid.tiers.empty()) return;

            const qreal chunkWidth = (bar.width() - (count - 1.0) * kChunkGap) / count;
            for (std::size_t i = 0; i < grid.tiers.size(); ++i) {
                const qreal left = bar.left() + static_cast<qreal>(i) * (chunkWidth + kChunkGap);
                appendTrackAndFill(items, QRectF(left, bar.top(), chunkWidth, bar.height()),
                                   fillWidth(summary.averageRankFills, chunkWidth, i),
                                   tierBarColor(grid.tiers[i], appearance), appearance);
            }

            // Tick k is the boundary where chunk k - 1 ends, so it sits in the gap rather than on a chunk's edge.
            const qreal tick = std::round(summary.completedRankPosition);
            if (tick < 0.0) return;
            const qreal x = tick >= count ? bar.right()
                            : tick == 0.0 ? bar.left()
                                          : bar.left() + tick * (chunkWidth + kChunkGap) - kChunkGap / 2.0;
            appendRect(items,
                       QRectF(x - kCompletedTickWidth / 2.0, bar.top() - kCompletedTickOverhang, kCompletedTickWidth,
                              bar.height() + 2.0 * kCompletedTickOverhang),
                       appearance.text);
        }

        Region summaryRegion(const TableCaptureSnapshot &snapshot, const TableCaptureSummary &summary,
                             const TableMetrics &metrics) {
            const Fonts &fonts = metrics.fonts;
            const CaptureAppearance &appearance = snapshot.appearance;
            const qreal captionHeight = lineHeight(fonts.small);
            const qreal valueHeight = lineHeight(fonts.bold);
            const qreal valueTop = captionHeight + kSummaryLineGap;
            const qreal detailTop = valueTop + valueHeight + kSummaryLineGap;

            Region region;
            qreal x = 0.0;
            const auto appendCell = [&](const QString &caption, const QString &value, const QString &color,
                                        const QString &detail = QString(), const qreal minimumWidth = 0.0) {
                const qreal width = std::max({minimumWidth, measuredWidth(fonts.small, caption),
                                              measuredWidth(fonts.bold, value), measuredWidth(fonts.small, detail)});
                appendText(region.items, QRectF(x, 0.0, width, captionHeight), caption, CaptureTextRole::Small,
                           appearance.placeholderText, kLeftAligned);
                if (!value.isEmpty()) {
                    appendText(region.items, QRectF(x, valueTop, width, valueHeight), value, CaptureTextRole::BodyBold,
                               color.isEmpty() ? appearance.text : QColor(color), kLeftAligned);
                }
                if (!detail.isEmpty()) {
                    appendText(region.items, QRectF(x, detailTop, width, captionHeight), detail, CaptureTextRole::Small,
                               appearance.placeholderText, kLeftAligned);
                }
                const Column cell{x, width};
                x += width + kSummaryCellGap;
                return cell;
            };

            const auto tierCount = static_cast<qreal>(snapshot.grid.tiers.size());
            const qreal minimumBarWidth = tierCount * kSummaryChunkMinimum;

            appendCell(summary.captions.attainedRank, summary.attainedRank, summary.attainedRankColor);
            appendCell(summary.captions.completedRank, summary.completedRank, summary.completedRankColor);
            const Column average = appendCell(summary.captions.averageRank, summary.averageRank, QString(), QString(),
                                              minimumBarWidth);
            appendCell(summary.captions.totalPlaytime, summary.totalPlaytime, QString());
            appendCell(summary.captions.nextTier, summary.nextTier, summary.nextTierColor,
                       summary.nextTierBlockerText);

            const QRectF bar(average.x, detailTop + kCompletedTickOverhang, average.width, barHeight(fonts));
            appendAverageRankBar(region.items, summary, snapshot.grid, appearance, bar);

            const qreal detailBottom = detailTop + captionHeight;
            const qreal bottom = snapshot.grid.tiers.empty()
                                     ? detailBottom
                                     : std::max(detailBottom, bar.bottom() + kCompletedTickOverhang);
            region.size = QSizeF(x - kSummaryCellGap, bottom);
            return region;
        }

        void moveItem(CaptureItem &item, const QPointF &offset) {
            std::visit([&offset](auto &held) { held.rect.translate(offset); }, item);
        }
    }

    CaptureLayout layoutTableCapture(const TableCaptureSnapshot &snapshot, const bool includeSummary) {
        const CaptureAppearance &appearance = snapshot.appearance;
        const TableMetrics metrics = measureTable(snapshot);

        Region summary;
        if (includeSummary && snapshot.summary && snapshot.completenessNote.isEmpty()) {
            summary = summaryRegion(snapshot, *snapshot.summary, metrics);
        }

        // The title may wrap at the table's width only when the summary does not already make the page wider.
        const qreal contentWidth = std::max(metrics.width(), summary.size.width());

        std::vector<Region> regions;
        regions.push_back(wrappedTextRegion(snapshot.benchmarkName, CaptureTextRole::Title, appearance.text,
                                            appearance.font, contentWidth));
        regions.push_back(wrappedTextRegion(snapshot.completenessNote, CaptureTextRole::Context,
                                            appearance.placeholderText, appearance.font, contentWidth));
        regions.push_back(std::move(summary));
        regions.push_back(tableRegion(snapshot, metrics));
        regions.push_back(keyRegion(snapshot, metrics, metrics.width()));

        CaptureLayout layout;
        layout.background = snapshot.appearance.window;
        layout.font = snapshot.appearance.font;

        qreal width = 0.0;
        qreal y = kPageMargin;
        for (Region &region : regions) {
            if (region.items.empty()) continue;
            if (y > kPageMargin) y += kSectionGap;
            for (CaptureItem &item : region.items) {
                moveItem(item, QPointF(kPageMargin, y));
                layout.items.push_back(std::move(item));
            }
            width = std::max(width, region.size.width());
            y += region.size.height();
        }
        layout.size = QSizeF(width + 2.0 * kPageMargin, y + kPageMargin);
        return layout;
    }
}
