#pragma once

#include <QDir>
#include <QFileInfo>
#include <QQmlAbstractUrlInterceptor>
#include <QString>
#include <QUrl>

namespace ksv::gallery {
    // Maps the compiled KovaaksStatsViewer module's qrc: QML URLs back to the
    // source tree, so dev tools run the QML on disk: qmlpreview hot-reloads it
    // (ksv_gallery) and a restart picks up edits without a rebuild (ksv_driver).
    class QmlSourceInterceptor final : public QQmlAbstractUrlInterceptor {
    public:
        explicit QmlSourceInterceptor(QString sourceDirectory)
            : m_sourceDirectory(std::move(sourceDirectory)) {}

        QUrl intercept(const QUrl& url, DataType type) override {
            constexpr auto modulePrefix = "/qt/qml/KovaaksStatsViewer/qml/";
            if (type != QmlFile || url.scheme() != "qrc" || !url.path().startsWith(modulePrefix)) {
                return url;
            }

            const QString relativePath = url.path().mid(QString::fromLatin1(modulePrefix).size());
            const QString sourcePath = QDir(m_sourceDirectory).filePath(relativePath);
            return QFileInfo::exists(sourcePath) ? QUrl::fromLocalFile(sourcePath) : url;
        }

    private:
        QString m_sourceDirectory;
    };
}
