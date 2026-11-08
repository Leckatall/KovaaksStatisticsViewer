#pragma once

#include <QColor>
#include <QDate>
#include <QFont>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <utility>
#include <vector>

#include "capture/capture_snapshot.h"
#include "presentation/benchmarks/tracking/progress_grid_content.h"

namespace ksv::ui {
    inline CaptureAppearance tableFixtureAppearance() {
        CaptureAppearance appearance;
        appearance.window = QColor(16, 24, 32);
        appearance.base = QColor(28, 36, 44);
        appearance.text = QColor(240, 240, 240);
        appearance.button = QColor(48, 56, 64);
        appearance.buttonText = QColor(230, 232, 234);
        appearance.mid = QColor(96, 104, 112);
        appearance.placeholderText = QColor(160, 160, 160);
        appearance.highlight = QColor(64, 160, 255);
        appearance.attention = QColor(255, 176, 32);
        appearance.groupTintAlpha = 0.25;

        const QFont app = QGuiApplication::font();
        appearance.font.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
        appearance.font.pixelSize = 14;
        return appearance;
    }

    inline QString tableFixtureTint(const int index) {
        return QColor::fromHsv((index * 67) % 360, 180, 210).name();
    }

    inline std::vector<presentation::ProgressGridTier> tableFixtureTiers(const int count) {
        static const QStringList names{
            QStringLiteral("Iron"), QStringLiteral("Bronze"), QStringLiteral("Silver"), QStringLiteral("Gold"),
            QStringLiteral("Platinum"), QStringLiteral("Diamond"), QStringLiteral("Jade"), QStringLiteral("Master"),
        };
        std::vector<presentation::ProgressGridTier> tiers;
        for (int i = 0; i < count; ++i) {
            presentation::ProgressGridTier tier;
            tier.id = names.at(i).toLower();
            tier.name = names.at(i);
            tier.color = QColor::fromHsv((i * 43) % 360, 120, 245).name();
            tier.barColor = QColor::fromHsv((i * 43) % 360, 230, 200).name();
            tiers.push_back(tier);
        }
        return tiers;
    }

    // Every formatted value is derived from `index` so no two rows, and no two cells of a row, share a text.
    inline presentation::ProgressGridScenarioRow tableFixtureRow(
            const QString &name, const int index, const std::vector<presentation::ProgressGridTier> &tiers) {
        presentation::ProgressGridScenarioRow row;
        row.entryId = QStringLiteral("entry-%1").arg(index);
        row.name = name;
        row.personalBestText = QString::number(12000 + index);
        row.personalBestDateText = QDate(2026, 1, 1).addDays(index).toString(Qt::ISODate);
        row.playCountText = QString::number(100 + index);
        row.hasPersonalBest = true;
        row.hasPersonalBestDate = true;
        row.hasPlayCount = true;

        const int tierCount = static_cast<int>(tiers.size());
        const int attained = tierCount > 0 ? index % tierCount : 0;
        for (int tier = 0; tier < tierCount; ++tier) {
            row.fills.push_back(tier < attained ? 1.0 : tier == attained ? 0.5 : 0.0);
            row.missingChunks.push_back(false);
            row.thresholdTexts.push_back(QString::number(500 * (tier + 1) + index));
        }
        if (tierCount > 0) {
            row.attainedTierText = tiers.at(attained).name;
            row.attainedTierColor = tiers.at(attained).color;
        }

        if (attained + 1 < tierCount) {
            row.nextTierText = tiers.at(attained + 1).name;
            row.nextTierColor = tiers.at(attained + 1).color;
            row.nextThresholdText = QString::number(7000 + index);
            row.nextText = row.nextTierText + QLatin1Char(' ') + row.nextThresholdText;
        } else {
            row.nextThresholdText = QStringLiteral("Highest tier attained");
            row.nextText = row.nextThresholdText;
        }
        return row;
    }

    inline presentation::ProgressGridGroup tableFixtureGroup(
            const QString &name, const int start, const int span, const QString &tint,
            const presentation::ProgressGridGroupColumn column = presentation::ProgressGridGroupColumn::Category) {
        presentation::ProgressGridGroup group;
        group.column = column;
        group.id = name;
        group.name = name;
        group.color = tint;
        group.start = start;
        group.span = span;
        return group;
    }

    inline TableCaptureSnapshot tableFixtureSnapshot(const QStringList &rowNames,
                                                     std::vector<presentation::ProgressGridGroup> groups,
                                                     const int tierCount = 3) {
        TableCaptureSnapshot snapshot;
        snapshot.benchmarkName = QStringLiteral("Fixture Benchmark");
        snapshot.appearance = tableFixtureAppearance();
        snapshot.grid.tiers = tableFixtureTiers(tierCount);
        for (int i = 0; i < rowNames.size(); ++i) {
            snapshot.grid.rows.push_back(tableFixtureRow(rowNames.at(i), i, snapshot.grid.tiers));
        }
        snapshot.grid.groups = std::move(groups);
        snapshot.grid.hasSubcategories = std::ranges::any_of(
            snapshot.grid.groups, [](const presentation::ProgressGridGroup &group) {
                return group.column == presentation::ProgressGridGroupColumn::Subcategory;
            });
        return snapshot;
    }

    // For a table of three tiers: a rank of 2.4, with 2 completed.
    inline TableCaptureSummary tableFixtureSummary(const std::vector<presentation::ProgressGridTier> &tiers) {
        TableCaptureSummary summary;
        summary.attainedRank = tiers.at(2).name;
        summary.attainedRankColor = tiers.at(2).color;
        summary.completedRank = tiers.at(1).name;
        summary.completedRankColor = tiers.at(1).color;
        summary.averageRank = QStringLiteral("%1, 40% to %2").arg(tiers.at(1).name, tiers.at(2).name);
        summary.averageRankFills = {1.0, 1.0, 0.4};
        summary.completedRankPosition = 2.0;
        summary.totalPlaytime = QStringLiteral("12h 30m");
        summary.nextTier = tiers.at(2).name;
        summary.nextTierColor = tiers.at(2).color;
        summary.nextTierBlockerText = QStringLiteral("Held back by two scenarios");
        return summary;
    }

    // 40 rows under 8 tiers, with two categories split into subcategories and one spanning category.
    inline TableCaptureSnapshot largeTableFixture() {
        using presentation::ProgressGridGroupColumn;

        QStringList rowNames;
        for (int i = 0; i < 40; ++i) rowNames.append(QStringLiteral("Tracking Drill %1").arg(i));

        presentation::ProgressGridGroup spanning =
            tableFixtureGroup(QStringLiteral("Dynamic Clicking"), 32, 8, tableFixtureTint(6));
        spanning.spansSubcategory = true;

        return tableFixtureSnapshot(
            rowNames,
            {
                tableFixtureGroup(QStringLiteral("Tracking"), 0, 16, tableFixtureTint(0)),
                tableFixtureGroup(QStringLiteral("Clicking"), 16, 16, tableFixtureTint(1)),
                spanning,
                tableFixtureGroup(QStringLiteral("Smooth"), 0, 8, tableFixtureTint(2),
                                  ProgressGridGroupColumn::Subcategory),
                tableFixtureGroup(QStringLiteral("Reactive"), 8, 8, tableFixtureTint(3),
                                  ProgressGridGroupColumn::Subcategory),
                tableFixtureGroup(QStringLiteral("Static"), 16, 8, tableFixtureTint(4),
                                  ProgressGridGroupColumn::Subcategory),
                tableFixtureGroup(QStringLiteral("Evasive"), 24, 8, tableFixtureTint(5),
                                  ProgressGridGroupColumn::Subcategory),
            },
            8);
    }
}
