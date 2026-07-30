//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QAbstractTableModel>
#include <QPointF>
#include <qqmlintegration.h>
#include <ranges>

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
        enum Column { XColumn = 0, YColumn, ColumnCount };

        explicit GraphViewModel(QObject* parent = nullptr);

        [[nodiscard]] Q_INVOKABLE int rowCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : int(m_points.size());
        }

        [[nodiscard]] Q_INVOKABLE int columnCount(const QModelIndex &parent = {}) const override {
            return parent.isValid() ? 0 : ColumnCount;
        }

        [[nodiscard]] Q_INVOKABLE QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        void setPoints(QList<QPointF> points);

        Q_INVOKABLE void appendPoint(qreal x, qreal y);

        [[nodiscard]] qreal xMin() const { return m_xMin; }
        [[nodiscard]] qreal xMax() const { return m_xMax; }
        [[nodiscard]] qreal yMin() const { return m_yMin; }
        [[nodiscard]] qreal yMax() const { return m_yMax; }

        Q_INVOKABLE void recomputeBounds();

    signals:
        void boundsChanged();

    private:
        QList<QPointF> m_points;
        qreal m_xMin = 0.0;
        qreal m_xMax = 10.0;
        qreal m_yMin = 0.0;
        qreal m_yMax = 10.0;
    };
};


#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
