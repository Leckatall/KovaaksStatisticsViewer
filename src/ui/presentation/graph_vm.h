//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QAbstractTableModel>
#include <QColor>
#include <QPointF>
#include <QVariantList>
#include <QVariantMap>
#include <array>
#include <qqmlintegration.h>
#include <ranges>

#include "app/usecases/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public QAbstractTableModel {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(QVariantList plottableColumns READ plottableColumns CONSTANT)
        Q_PROPERTY(QVariantMap axisBounds READ axisBounds NOTIFY boundsChanged)
        Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)
        // Instance-accessible mirrors of the Column enum values DashboardGraph.qml
        // needs. Referencing the enum via the bare "GraphViewModel.Score"-style
        // static type name silently fails to resolve at runtime (ReferenceError)
        // from within this same QML module, even with a self-import, so QML reads
        // these off the graphVm instance it already has instead.
        Q_PROPERTY(int timeColumn READ timeColumn CONSTANT)
        Q_PROPERTY(int scoreColumn READ scoreColumn CONSTANT)
        Q_PROPERTY(int totalColumnCount READ totalColumnCount CONSTANT)

    public:
        enum Column { Time = 0, Score, Accuracy, Shots, Kills, Dmg, ColumnCount };
        Q_ENUM(Column)

        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : int(m_data.size());
        }

        [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : ColumnCount;
        }

        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        void setData(QList<QMap<Column, qreal>> data);

        // Columns other than Time, i.e. the ones that can be plotted as lines
        // against Time on the X axis. Exposed so QML can dynamically generate
        // one line per column instead of hard-coding each series.
        [[nodiscard]] QVariantList plottableColumns() const;

        // Maps each Column (keyed by its stringified enum value) to its
        // {min, max} axis bounds, stored as QPointF(min, max).
        [[nodiscard]] QVariantMap axisBounds() const;

        [[nodiscard]] static int timeColumn() { return Time; }
        [[nodiscard]] static int scoreColumn() { return Score; }
        [[nodiscard]] static int totalColumnCount() { return ColumnCount; }

        // Number of rows currently loaded. Series that need more than one
        // point to be well-defined (e.g. spline interpolation) should gate
        // their rendering on this rather than assuming data is always present.
        [[nodiscard]] int pointCount() const { return int(m_data.size()); }

        Q_INVOKABLE [[nodiscard]] QString columnName(Column column) const;
        Q_INVOKABLE [[nodiscard]] QColor columnColor(Column column) const;

        void recomputeBounds();

    public slots:
        void fetchData(const QString& scenario_id);

    signals:
        void boundsChanged();
        void pointCountChanged();

    private:
        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        QList<QMap<Column, qreal>> m_data;
        std::array<std::pair<qreal, qreal>, ColumnCount> m_bounds{};
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
