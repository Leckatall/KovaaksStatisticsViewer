//
// Entry point for the QML component tests (tst_*.qml in this directory).
// These drive real QML item trees (menu wiring, button clicks, checkbox
// bindings) via Qt Quick Test, as opposed to the gtest suite in
// tests/ui/*.cpp which only covers the plain-C++ view-model logic.
//

#include <QtQuickTest/quicktest.h>
#include <QtQml>
#include <QSettings>
#include <QTemporaryDir>

#include "usecases/i_session_controller.h"
#include "presentation/benchmark_table_model.h"
#include "qml_registration.h"

// Test-only: lets a JS fake manager hand BenchmarkEditorTable a real BenchmarkTableModel, since a
// TableView needs a genuine QAbstractItemModel. `setProjection` takes the plain-object shape built
// by TestDoubles.benchmarkTableProjection().
class BenchmarkTableModelFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)

public:
    explicit BenchmarkTableModelFixture(QObject *parent = nullptr) : QObject(parent), m_model(new ksv::presentation::BenchmarkTableModel(this)) {}

    [[nodiscard]] QAbstractItemModel *model() const { return m_model; }

    Q_INVOKABLE void setProjection(const QVariantMap &projection) {
        using namespace ksv::presentation;
        const auto group = [](const QVariant &value) {
            const auto map = value.toMap();
            return BenchmarkTableGroupSpan{map.value("id").toString(), map.value("name").toString(),
                                           map.value("color").value<QColor>(), map.value("spanStart").toInt(),
                                           map.value("spanLength").toInt(), map.value("issues").toStringList()};
        };
        BenchmarkTableProjection result;
        for (const auto &tier: projection.value("tiers").toList()) {
            const auto map = tier.toMap();
            result.tiers.push_back({map.value("id").toString(), map.value("name").toString(),
                                    map.value("color").value<QColor>(), map.value("issues").toStringList()});
        }
        for (const auto &row: projection.value("rows").toList()) {
            const auto map = row.toMap();
            BenchmarkTableRow built;
            built.entryId = map.value("entryId").toString();
            built.name = map.value("name").toString();
            built.scenarioIssues = map.value("scenarioIssues").toStringList();
            built.category = group(map.value("category"));
            built.subcategory = group(map.value("subcategory"));
            built.mappingState = map.value("mappingState").toString();
            built.mappingCandidates = map.value("mappingCandidates").toList();
            for (const auto &cell: map.value("cells").toList()) {
                const auto c = cell.toMap();
                built.cells.push_back({c.value("displayText").toString(), c.value("editText").toString(),
                                       c.value("hasValue").toBool(), c.value("inputState").toString(),
                                       c.value("issues").toStringList()});
            }
            result.rows.push_back(std::move(built));
        }
        m_model->setProjection(std::move(result));
    }

private:
    ksv::presentation::BenchmarkTableModel *m_model;
};

class UiQmlTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *) {
        // QML `Settings {}` items (VisualSettingsManager.qml) need these to construct at
        // all; without them QSettings fails to initialize and every Settings-backed
        // property silently behaves as a plain, non-persisted local property instead.
        QCoreApplication::setOrganizationName("Lecka");
        QCoreApplication::setApplicationName("KovaaksStatsViewer");
        static QTemporaryDir settings_dir;
        if (settings_dir.isValid()) {
            QSettings::setDefaultFormat(QSettings::IniFormat);
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
            QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());
        }

        qRegisterMetaType<ksv::application::ISessionController *>();
        ksv::declare_metatypes();
        qmlRegisterType<BenchmarkTableModelFixture>("KsvTestSupport", 1, 0, "BenchmarkTableModelFixture");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui_qml_tests, UiQmlTestSetup)

#include "tst_ui_qml.moc"
