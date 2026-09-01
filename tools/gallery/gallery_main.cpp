//
// Dev-only component gallery: builds the real App wiring against a throwaway
// KovaaKs dir seeded with a generated profile, then renders UI components in
// Gallery.qml. Loaded by file path so `qmlpreview` hot-reloads it.
// Not shipped — gated behind KSV_BUILD_GALLERY.
//

#include <memory>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>

#include <app/app.h>

#include "qml_registration.h"
#include "gallery_dataset.h"
#include "settings_service.h"
#include "series_config_store.h"
#include "formats/protobuf/proto_decoder.h"
#include "formats/protobuf/profile_serializer.h"

using namespace ksv;

namespace {
    QVariantList createDatasetProfiles(const QString &directory) {
        const std::array datasets{
            gallery::Dataset::RichProfile,
            gallery::Dataset::SingleRun,
            gallery::Dataset::ShortHistory,
            gallery::Dataset::BunchedHistory,
        };
        data::ProfileSerializer serializer;
        QVariantList paths;
        for (std::size_t index = 0; index < datasets.size(); ++index) {
            const auto path = QDir(directory).absoluteFilePath(QStringLiteral("gallery-profile-%1.pb").arg(index));
            if (!serializer.save(gallery::makeProfile(datasets[index]), path.toStdString())) return {};
            paths.append(QUrl::fromLocalFile(path));
        }
        return paths;
    }

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
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer-Gallery");

    QTemporaryDir tmp;
    const QVariantList datasetPaths = createDatasetProfiles(tmp.path());
    if (datasetPaths.size() != static_cast<int>(gallery::kDatasetNames.size())) return -1;

    auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
    settings->setKovaaksDirs({tmp.path().toStdString()});
    settings->setProfilePath(datasetPaths.front().toUrl().toLocalFile().toStdString());

    application::App app(
        settings, std::make_shared<data::ProtoDecoder>(),
        std::make_shared<qt_data::SeriesConfigStore>(settings));
    QQmlApplicationEngine* engine = app.engine();
    GalleryQmlSourceInterceptor sourceInterceptor(QStringLiteral(GALLERY_QML_SOURCE_DIR));
    engine->addUrlInterceptor(&sourceInterceptor);

    // qmlpreview recreates the root object without replaying the engine's
    // initial properties. Root-context properties remain available to each
    // replacement object, so live reload keeps using the real view models.
    engine->rootContext()->setContextProperty("galleryGraphVm", app.graphVm());
    engine->rootContext()->setContextProperty("galleryPlaytimeVm", app.playtimeVm());
    engine->rootContext()->setContextProperty("galleryHistoryVm", app.completionHistoryVm());
    engine->rootContext()->setContextProperty("gallerySessionVm", app.sessionVm());
    engine->rootContext()->setContextProperty("gallerySettingsVm", app.settingsVm());
    engine->rootContext()->setContextProperty("galleryScenarioBrowserVm", app.scenarioBrowserVm());
    QStringList datasetNames;
    for (const auto name: gallery::kDatasetNames) datasetNames.append(QString::fromUtf8(name));
    engine->rootContext()->setContextProperty("galleryDatasetNames", datasetNames);
    engine->rootContext()->setContextProperty("galleryDatasetPaths", datasetPaths);
    engine->load(QUrl::fromLocalFile(GALLERY_QML_FILE));
    if (engine->rootObjects().isEmpty()) return -1;

    return QGuiApplication::exec();
}
