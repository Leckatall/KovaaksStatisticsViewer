//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QAbstractTableModel>
#include <QPointF>
#include <qqmlintegration.h>
#include <ranges>

#include "app/usecases/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public QAbstractTableModel {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(qreal xMin READ xMin NOTIFY boundsChanged)
        Q_PROPERTY(qreal xMax READ xMax NOTIFY boundsChanged)
        Q_PROPERTY(qreal yMin READ yMin NOTIFY boundsChanged)
        Q_PROPERTY(qreal yMax READ yMax NOTIFY boundsChanged)

    public:
        enum Column { Time = 0, Score, Accuracy, ColumnCount };

        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : int(m_data.size());
        }

        [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : ColumnCount;
        }

        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        void setData(QList<QMap<Column, qreal>> data);

        void setColumn(Column column, QList<qreal> col_data);


        [[nodiscard]] qreal xMin() const { return m_xMin; }
        [[nodiscard]] qreal xMax() const { return m_xMax; }
        [[nodiscard]] qreal yMin() const { return m_yMin; }
        [[nodiscard]] qreal yMax() const { return m_yMax; }

        void recomputeBounds();

    public slots:
        void fetchData(const QString& scenario_id);

    signals:
        void boundsChanged();

    private:
        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        QList<QMap<Column, qreal>> m_data;
        qreal m_xMin = 0.0;
        qreal m_xMax = 10.0;
        qreal m_yMin = 0.0;
        qreal m_yMax = 10.0;
    };
};


#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
