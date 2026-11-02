#include "capture_preview_item.h"

#include <QPainter>

namespace ksv::ui {
    CapturePreviewItem::CapturePreviewItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    }

    void CapturePreviewItem::setImage(const QImage &image) {
        if (m_image == image) return;
        m_image = image;
        update();
        emit imageChanged();
    }

    void CapturePreviewItem::paint(QPainter *painter) {
        if (m_image.isNull()) return;
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(boundingRect(), m_image);
    }
}
