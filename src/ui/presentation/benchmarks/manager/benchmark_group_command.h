#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_GROUP_COMMAND_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_GROUP_COMMAND_H

#include <QString>
#include <vector>

#include "benchmarks/benchmark.h"

namespace ksv::presentation {
    // `targetId` is empty when the destination is Uncategorized.
    struct GroupCommandAction {
        enum class Kind { Create, Move };

        Kind kind = Kind::Move;
        QString name;
        QString parentId;
        QString targetId;
        QString path;
    };

    struct GroupCommandOptions {
        std::vector<GroupCommandAction> actions;
        bool exactMatch = false;
    };

    // Destinations are every category and subcategory in hierarchy order, then Uncategorized. A
    // destination is exact when its name equals the query and it sits at the level `parentCategoryId`
    // selects (top level when empty); a creation is only offered when nothing is exact.
    GroupCommandOptions buildGroupCommandOptions(const domain::Benchmark &draft, const QString &query,
                                                 const QString &parentCategoryId);
}

#endif
