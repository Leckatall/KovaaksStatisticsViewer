#include "presentation/benchmarks/manager/benchmark_group_command.h"

#include <QObject>

namespace ksv::presentation {
    namespace {
        struct Destination {
            QString id;
            QString name;
            QString path;
            QString parentId;
        };

        std::vector<Destination> destinationsOf(const domain::Benchmark &draft) {
            std::vector<Destination> destinations;
            for (const auto &category: draft.categories) {
                const auto categoryName = QString::fromStdString(category.name);
                destinations.push_back({QString::fromStdString(category.id.value), categoryName, categoryName, {}});
                for (const auto &sub: category.subcategories) {
                    const auto subName = QString::fromStdString(sub.name);
                    destinations.push_back({QString::fromStdString(sub.id.value), subName,
                                            categoryName + QStringLiteral(" › ") + subName,
                                            QString::fromStdString(category.id.value)});
                }
            }
            const auto uncategorized = QObject::tr("Uncategorized");
            destinations.push_back({{}, uncategorized, uncategorized, {}});
            return destinations;
        }

        GroupCommandAction moveTo(const Destination &destination) {
            GroupCommandAction action;
            action.kind = GroupCommandAction::Kind::Move;
            action.targetId = destination.id;
            action.path = destination.path;
            return action;
        }

        bool containsInsensitive(const QString &text, const QString &needle) {
            return text.contains(needle, Qt::CaseInsensitive);
        }
    }

    GroupCommandOptions buildGroupCommandOptions(const domain::Benchmark &draft, const QString &query,
                                                 const QString &parentCategoryId) {
        const auto destinations = destinationsOf(draft);
        const auto trimmed = query.trimmed();
        GroupCommandOptions options;

        if (trimmed.isEmpty()) {
            for (const auto &destination: destinations) options.actions.push_back(moveTo(destination));
            return options;
        }

        const auto firstWord = trimmed.section(QChar::Space, 0, 0, QString::SectionSkipEmpty);
        std::vector<const Destination *> exact;
        std::vector<const Destination *> others;
        for (const auto &destination: destinations) {
            if (destination.name.compare(trimmed, Qt::CaseInsensitive) == 0 && destination.parentId == parentCategoryId)
                exact.push_back(&destination);
            else if (containsInsensitive(destination.path, trimmed) || containsInsensitive(destination.path, firstWord))
                others.push_back(&destination);
        }

        options.exactMatch = !exact.empty();
        if (!options.exactMatch) {
            GroupCommandAction create;
            create.kind = GroupCommandAction::Kind::Create;
            create.name = trimmed;
            create.parentId = parentCategoryId;
            options.actions.push_back(std::move(create));
        }
        for (const auto *destination: exact) options.actions.push_back(moveTo(*destination));
        for (const auto *destination: others) options.actions.push_back(moveTo(*destination));
        return options;
    }
}
