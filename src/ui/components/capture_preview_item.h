#ifndef KOVAAKSSTATSVIEWER_CAPTURE_PREVIEW_ITEM_H
#define KOVAAKSSTATSVIEWER_CAPTURE_PREVIEW_ITEM_H

#include <QImage>
#include <QQuickPaintedItem>
#include <qqmlintegration.h>

namespace ksv::ui {
    // Display only: the image scales to the item's bounds and is never the output.
    class CapturePreviewItem : public QQuickPaintedItem {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)

    public:
        explicit CapturePreviewItem(QQuickItem *parent = nullptr);

        [[nodiscard]] QImage image() const { return m_image; }
        void setImage(const QImage &image);

        void paint(QPainter *painter) override;

    signals:
        void imageChanged();

    private:
        QImage m_image;
    };
}

#endif
