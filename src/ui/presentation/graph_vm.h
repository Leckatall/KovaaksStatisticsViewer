//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QObject>
#include <QColor>
#include <QPointF>
#include <QVariantList>
#include <QVariantMap>
#include <array>
#include <qqmlintegration.h>
#include <ranges>

#include "app/usecases/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(QVariantList plottableColumns READ plottableColumns CONSTANT)
        Q_PROPERTY(QVariantMap axisBounds READ axisBounds NOTIFY boundsChanged)
        Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)

    public:
        enum Column { Time = 0, Score, Accuracy, Shots, Kills, Dmg, ColumnCount };
        Q_ENUM(Column)

        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        void setData(QList<QMap<Column, qreal>> data);

        [[nodiscard]] QVariantList plottableColumns() const;

        [[nodiscard]] QVariantMap axisBounds() const;

        [[nodiscard]] int pointCount() const { return int(m_data.size()); }

        [[nodiscard]] QString scenarioTitle() const { return m_scenarioTitle; }

        Q_INVOKABLE [[nodiscard]] QString columnName(Column column) const;
        Q_INVOKABLE [[nodiscard]] QColor columnColor(Column column) const;
        
        [[nodiscard]] QList<QPointF> seriesPoints(Column column) const;

        void recomputeBounds();

    public slots:
        void fetchData(const QString& scenario_id);

    signals:
        void boundsChanged();
        void pointCountChanged();
        void scenarioTitleChanged();

    private:
        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        QList<QMap<Column, qreal>> m_data;
        std::array<std::pair<qreal, qreal>, ColumnCount> m_bounds{};
        QString m_scenarioTitle;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
