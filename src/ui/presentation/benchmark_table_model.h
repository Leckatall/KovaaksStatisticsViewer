#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TABLE_MODEL_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

namespace ksv::presentation {
    enum class BenchmarkColumnKind { Category, Subcategory, Scenario, Threshold };

    struct BenchmarkTableCell {
        QString displayText;
        QString editText;
        bool hasValue = false;
        QString inputState; // "", "invalid" or "nonFinite"
        QStringList issues;
        friend bool operator==(const BenchmarkTableCell &, const BenchmarkTableCell &) = default;
    };

    struct BenchmarkTableGroupSpan {
        QString id; // empty for Uncategorized / no subcategory
        QString name;
        QColor color;
        int spanStart = 0;
        int spanLength = 0;
        QStringList issues;
        friend bool operator==(const BenchmarkTableGroupSpan &, const BenchmarkTableGroupSpan &) = default;
    };

    struct BenchmarkTableRow {
        QString entryId;
        QString name;
        QStringList scenarioIssues;
        BenchmarkTableGroupSpan category;
        BenchmarkTableGroupSpan subcategory;
        QString mappingState;
        QVariantList mappingCandidates;
        std::vector<BenchmarkTableCell> cells; // one per tier, in ladder order
        friend bool operator==(const BenchmarkTableRow &, const BenchmarkTableRow &) = default;
    };

    struct BenchmarkTableHeader {
        QString tierId;
        QString name;
        QColor color;
        QStringList issues;
        friend bool operator==(const BenchmarkTableHeader &, const BenchmarkTableHeader &) = default;
    };

    struct BenchmarkTableProjection {
        std::vector<BenchmarkTableHeader> tiers;
        std::vector<BenchmarkTableRow> rows;
    };

    // Flat projection of the manager's working copy: one row per scenario entry in hierarchy
    // order (Uncategorized last), Category/Subcategory/Scenario columns then one per tier. It is
    // derived from session state and owns none of it.
    class BenchmarkTableModel : public QAbstractTableModel {
        Q_OBJECT

    public:
        static constexpr int FixedColumnCount = 3;

        enum Role {
            EntryIdRole = Qt::UserRole + 1,
            TierIdRole,
            ColumnKindRole,
            DisplayTextRole,
            EditTextRole,
            HasValueRole,
            InputStateRole,
            IssuesRole,
            CategoryIdRole,
            CategoryNameRole,
            CategoryColorRole,
            CategorySpanStartRole,
            CategorySpanLengthRole,
            SubcategoryIdRole,
            SubcategoryNameRole,
            SubcategoryColorRole,
            SubcategorySpanStartRole,
            SubcategorySpanLengthRole,
            MappingStateRole,
            MappingCandidatesRole,
            HeaderColorRole,
        };
        Q_ENUM(Role)

        explicit BenchmarkTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

        // Resets only when the row or tier identity sequence changes, so delegates and focus
        // survive ordinary value edits.
        void setProjection(BenchmarkTableProjection projection);

        [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
        [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE int rowForEntry(const QString &entryId) const;
        Q_INVOKABLE int columnForTier(const QString &tierId) const;
        // -1 for an empty group, which occupies no row.
        Q_INVOKABLE int firstRowOfGroup(const QString &groupId) const;
        // The stable {entryId, tierId, columnKind} address of a cell; empty when out of range.
        Q_INVOKABLE QVariantMap anchorAt(int row, int column) const;
        // Row-level roles (entry, group spans, mapping) for contextual controls; empty when out of range.
        Q_INVOKABLE QVariantMap rowInfo(int row) const;

    private:
        BenchmarkTableProjection m_projection;
        QHash<QString, int> m_rowByEntry;
        QHash<QString, int> m_columnByTier;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_TABLE_MODEL_H
