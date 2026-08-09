//
// Dev-only component gallery: builds the real App wiring against a throwaway
// KovaaKs dir seeded with a checked-in .perf fixture, then renders every UI
// component in Gallery.qml. Loaded by file path so `qmlpreview` hot-reloads it.
// Not shipped — gated behind KSV_BUILD_GALLERY.
//

#include <memory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include <app/app.h>

#include "qml_registration.h"
#include "settings_service.h"
#include "formats/protobuf/proto_decoder.h"

using namespace ksv;

namespace {
    class GalleryQmlSourceInterceptor final : public QQmlAbstractUrlInterceptor {
    public:
        explicit GalleryQmlSourceInterceptor(QString sourceDirectory)
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

int main(int argc, char *argv[]) {
    declare_metatypes();
    QQuickStyle::setStyle("Fusion");
    QGuiApplication qapp(argc, argv);
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer-Gallery");

    QTemporaryDir tmp;
    const QString performancesDir = QDir(tmp.path()).absoluteFilePath("FPSAimTrainer/performances");
    QDir().mkpath(performancesDir);

    const QString fixtureName = "1wall6targets TE.perf";
    const QString perf = QDir(performancesDir).absoluteFilePath(fixtureName);
    QFile::copy(QDir(GALLERY_FIXTURE_DIR).absoluteFilePath(fixtureName), perf);

    auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
    settings->setKovaaksDir(tmp.path().toStdString());
    settings->setProfilePath(QDir(tmp.path()).absoluteFilePath("cache/profile_cache.pb").toStdString());

    application::App app(settings, std::make_shared<data::ProtoDecoder>());
    app.sessionVm()->generateProfile();
    app.graphVm()->fetchData(QUrl::fromLocalFile(perf).toString());

    QQmlApplicationEngine* engine = app.engine();
    GalleryQmlSourceInterceptor sourceInterceptor(QStringLiteral(GALLERY_QML_SOURCE_DIR));
    engine->addUrlInterceptor(&sourceInterceptor);

    // qmlpreview recreates the root object without replaying the engine's
    // initial properties. Root-context properties remain available to each
    // replacement object, so live reload keeps using the real view models.
    engine->rootContext()->setContextProperty("galleryGraphVm", app.graphVm());
    engine->rootContext()->setContextProperty("galleryPlaytimeVm", app.playtimeVm());
    engine->rootContext()->setContextProperty("gallerySessionVm", app.sessionVm());
    engine->rootContext()->setContextProperty("gallerySettingsVm", app.settingsVm());
    engine->rootContext()->setContextProperty("galleryScenarioBrowserVm", app.scenarioBrowserVm());
    engine->load(QUrl::fromLocalFile(GALLERY_QML_FILE));
    if (engine->rootObjects().isEmpty()) return -1;

    return QGuiApplication::exec();
}
