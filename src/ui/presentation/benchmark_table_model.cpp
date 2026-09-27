#include "benchmark_table_model.h"

#include <utility>

namespace ksv::presentation {
    namespace {
        bool sameShape(const BenchmarkTableProjection &a, const BenchmarkTableProjection &b) {
            if (a.rows.size() != b.rows.size() || a.tiers.size() != b.tiers.size()) return false;
            for (std::size_t i = 0; i < a.rows.size(); ++i)
                if (a.rows[i].entryId != b.rows[i].entryId) return false;
            for (std::size_t i = 0; i < a.tiers.size(); ++i)
                if (a.tiers[i].tierId != b.tiers[i].tierId) return false;
            return true;
        }

        // `offset` is the role's distance from the group's Id role; the five group roles are
        // declared contiguously in Id, Name, Color, SpanStart, SpanLength order.
        QVariant groupRole(const BenchmarkTableGroupSpan &group, int offset) {
            switch (offset) {
                case 0: return group.id;
                case 1: return group.name;
                case 2: return group.color;
                case 3: return group.spanStart;
                case 4: return group.spanLength;
            }
            return {};
        }
    }

    void BenchmarkTableModel::setProjection(BenchmarkTableProjection projection) {
        if (!sameShape(m_projection, projection)) {
            beginResetModel();
            m_projection = std::move(projection);
            m_rowByEntry.clear();
            m_columnByTier.clear();
            for (int row = 0; row < static_cast<int>(m_projection.rows.size()); ++row)
                m_rowByEntry.insert(m_projection.rows[static_cast<std::size_t>(row)].entryId, row);
            for (int tier = 0; tier < static_cast<int>(m_projection.tiers.size()); ++tier)
                m_columnByTier.insert(m_projection.tiers[static_cast<std::size_t>(tier)].tierId, FixedColumnCount + tier);
            endResetModel();
            return;
        }
        // Same ids in the same order, so the lookup maps stand; only rows and headers that differ
        // are announced, keeping delegate rebinding to what an edit actually touched.
        const auto previous = std::exchange(m_projection, std::move(projection));
        for (int row = 0; row < rowCount(); ++row)
            if (m_projection.rows[static_cast<std::size_t>(row)] != previous.rows[static_cast<std::size_t>(row)])
                emit dataChanged(index(row, 0), index(row, columnCount() - 1));
        if (m_projection.tiers != previous.tiers) emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }

    int BenchmarkTableModel::rowCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : static_cast<int>(m_projection.rows.size());
    }

    int BenchmarkTableModel::columnCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : FixedColumnCount + static_cast<int>(m_projection.tiers.size());
    }

    QVariant BenchmarkTableModel::data(const QModelIndex &index, int role) const {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) return {};
        const auto &row = m_projection.rows[static_cast<std::size_t>(index.row())];
        const int column = index.column();
        const auto kind = column >= FixedColumnCount ? BenchmarkColumnKind::Threshold
                                                     : static_cast<BenchmarkColumnKind>(column);
        const auto tierIndex = static_cast<std::size_t>(column - FixedColumnCount);
        const BenchmarkTableCell *cell = kind == BenchmarkColumnKind::Threshold ? &row.cells[tierIndex] : nullptr;
        switch (role) {
            case EntryIdRole: return row.entryId;
            case TierIdRole: return cell ? m_projection.tiers[tierIndex].tierId : QString();
            case ColumnKindRole: return static_cast<int>(kind);
            case Qt::DisplayRole:
            case DisplayTextRole:
                switch (kind) {
                    case BenchmarkColumnKind::Category: return row.category.name;
                    case BenchmarkColumnKind::Subcategory: return row.subcategory.name;
                    case BenchmarkColumnKind::Scenario: return row.name;
                    case BenchmarkColumnKind::Threshold: return cell->displayText;
                }
                return {};
            case Qt::EditRole:
            case EditTextRole: return cell ? cell->editText : data(index, DisplayTextRole);
            case HasValueRole: return cell ? cell->hasValue : !row.name.isEmpty();
            case InputStateRole: return cell ? cell->inputState : QString();
            case IssuesRole:
                switch (kind) {
                    case BenchmarkColumnKind::Category: return row.category.issues;
                    case BenchmarkColumnKind::Subcategory: return row.subcategory.issues;
                    case BenchmarkColumnKind::Scenario: return row.scenarioIssues;
                    case BenchmarkColumnKind::Threshold: return cell->issues;
                }
                return {};
            case CategoryIdRole:
            case CategoryNameRole:
            case CategoryColorRole:
            case CategorySpanStartRole:
            case CategorySpanLengthRole: return groupRole(row.category, role - CategoryIdRole);
            case SubcategoryIdRole:
            case SubcategoryNameRole:
            case SubcategoryColorRole:
            case SubcategorySpanStartRole:
            case SubcategorySpanLengthRole: return groupRole(row.subcategory, role - SubcategoryIdRole);
            case MappingStateRole: return row.mappingState;
            case MappingCandidatesRole: return row.mappingCandidates;
            default: return {};
        }
    }

    QVariant BenchmarkTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
        if (orientation != Qt::Horizontal || section < 0 || section >= columnCount()) return {};
        if (section < FixedColumnCount) {
            if (role == ColumnKindRole) return section;
            if (role != Qt::DisplayRole && role != DisplayTextRole) return {};
            switch (static_cast<BenchmarkColumnKind>(section)) {
                case BenchmarkColumnKind::Category: return tr("Category");
                case BenchmarkColumnKind::Subcategory: return tr("Subcategory");
                default: return tr("Scenario");
            }
        }
        const auto &tier = m_projection.tiers[static_cast<std::size_t>(section - FixedColumnCount)];
        switch (role) {
            case Qt::DisplayRole:
            case DisplayTextRole: return tier.name;
            case TierIdRole: return tier.tierId;
            case ColumnKindRole: return static_cast<int>(BenchmarkColumnKind::Threshold);
            case HeaderColorRole: return tier.color;
            case IssuesRole: return tier.issues;
            default: return {};
        }
    }

    Qt::ItemFlags BenchmarkTableModel::flags(const QModelIndex &index) const {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) return Qt::NoItemFlags;
        const auto &row = m_projection.rows[static_cast<std::size_t>(index.row())];
        // Uncategorized and "no subcategory" have no user name to edit.
        const bool editable = index.column() >= static_cast<int>(BenchmarkColumnKind::Scenario) ||
                              (index.column() == static_cast<int>(BenchmarkColumnKind::Category) && !row.category.id.isEmpty()) ||
                              (index.column() == static_cast<int>(BenchmarkColumnKind::Subcategory) && !row.subcategory.id.isEmpty());
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | (editable ? Qt::ItemIsEditable : Qt::NoItemFlags);
    }

    int BenchmarkTableModel::firstRowOfGroup(const QString &groupId) const {
        if (groupId.isEmpty()) return -1;
        for (int row = 0; row < static_cast<int>(m_projection.rows.size()); ++row) {
            const auto &projected = m_projection.rows[static_cast<std::size_t>(row)];
            if (projected.category.id == groupId || projected.subcategory.id == groupId) return row;
        }
        return -1;
    }

    QVariantMap BenchmarkTableModel::anchorAt(int row, int column) const {
        const auto cell = index(row, column);
        if (!cell.isValid()) return {};
        return {{"entryId", data(cell, EntryIdRole)},
                {"tierId", data(cell, TierIdRole)},
                {"columnKind", data(cell, ColumnKindRole)}};
    }

    QVariantMap BenchmarkTableModel::rowInfo(int row) const {
        if (row < 0 || row >= rowCount()) return {};
        const auto &projected = m_projection.rows[static_cast<std::size_t>(row)];
        return {{"row", row},
                {"entryId", projected.entryId},
                {"name", projected.name},
                {"categoryId", projected.category.id},
                {"categorySpanStart", projected.category.spanStart},
                {"categorySpanLength", projected.category.spanLength},
                {"subcategoryId", projected.subcategory.id},
                {"subcategorySpanStart", projected.subcategory.spanStart},
                {"subcategorySpanLength", projected.subcategory.spanLength},
                {"mappingState", projected.mappingState},
                {"mappingCandidates", projected.mappingCandidates}};
    }

    int BenchmarkTableModel::rowForEntry(const QString &entryId) const { return m_rowByEntry.value(entryId, -1); }

    int BenchmarkTableModel::columnForTier(const QString &tierId) const { return m_columnByTier.value(tierId, -1); }

    QHash<int, QByteArray> BenchmarkTableModel::roleNames() const {
        return {
            {Qt::DisplayRole, "display"},
            {EntryIdRole, "entryId"},
            {TierIdRole, "tierId"},
            {ColumnKindRole, "columnKind"},
            {DisplayTextRole, "displayText"},
            {EditTextRole, "editText"},
            {HasValueRole, "hasValue"},
            {InputStateRole, "inputState"},
            {IssuesRole, "issues"},
            {CategoryIdRole, "categoryId"},
            {CategoryNameRole, "categoryName"},
            {CategoryColorRole, "categoryColor"},
            {CategorySpanStartRole, "categorySpanStart"},
            {CategorySpanLengthRole, "categorySpanLength"},
            {SubcategoryIdRole, "subcategoryId"},
            {SubcategoryNameRole, "subcategoryName"},
            {SubcategoryColorRole, "subcategoryColor"},
            {SubcategorySpanStartRole, "subcategorySpanStart"},
            {SubcategorySpanLengthRole, "subcategorySpanLength"},
            {MappingStateRole, "mappingState"},
            {MappingCandidatesRole, "mappingCandidates"},
            {HeaderColorRole, "headerColor"},
        };
    }
}
