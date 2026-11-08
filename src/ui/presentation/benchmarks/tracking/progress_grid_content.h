#ifndef KOVAAKSSTATSVIEWER_PROGRESS_GRID_CONTENT_H
#define KOVAAKSSTATSVIEWER_PROGRESS_GRID_CONTENT_H

#include <QObject>
#include <QString>

#include <optional>
#include <vector>

namespace ksv::presentation {
    enum class ProgressGridEdgeState { None, Unplayed, Unmatched, AboveTop };

    enum class ProgressGridGroupColumn { Category, Subcategory };

    // Colours are "#rrggbb", or empty when the cell has none.
    struct ProgressGridTier {
        QString id;
        QString name;
        QString color;
        QString barColor;
    };

    struct ProgressGridScenarioRow {
        QString entryId;
        QString name;
        QString personalBestText;
        QString personalBestDateText;
        QString playCountText;
        bool hasPersonalBest = false;
        bool hasPersonalBestDate = false;
        bool hasPlayCount = false;
        QString attainedTierText;
        QString attainedTierColor;
        std::vector<double> fills;
        std::optional<double> recentPosition;
        QString recentDescription;
        QString nextTierText;
        QString nextTierColor;
        QString nextThresholdText;
        QString nextText;
        QString matchStateText;
        QString matchStateHint;
        QString barDescription;
        std::vector<bool> missingChunks;
        std::vector<QString> thresholdTexts;
        ProgressGridEdgeState edgeState = ProgressGridEdgeState::None;
        QString statusText;
        QString aboveTopText;
        bool blocker = false;
        QString blockerColor;
        QString blockerText;
    };

    struct ProgressGridGroup {
        ProgressGridGroupColumn column = ProgressGridGroupColumn::Category;
        QString id;
        QString name;
        QString color;
        int start = 0;
        int span = 0;
        bool spansSubcategory = false;
        QString rankText;
        QString rankColor;
        QString rankBarColor;
    };

    struct ProgressGridHeadings {
        QString category = QObject::tr("Category");
        QString subcategory = QObject::tr("Subcategory");
        QString scenario = QObject::tr("Scenario");
        QString progress = QObject::tr("Progress");
        QString pbDate = QObject::tr("PB date");
        QString plays = QObject::tr("Plays");
        QString next = QObject::tr("Next tier");
    };

    struct ProgressGridContent {
        ProgressGridHeadings headings;
        std::vector<ProgressGridTier> tiers;
        std::vector<ProgressGridScenarioRow> rows;
        std::vector<ProgressGridGroup> groups;
        bool hasSubcategories = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_PROGRESS_GRID_CONTENT_H
