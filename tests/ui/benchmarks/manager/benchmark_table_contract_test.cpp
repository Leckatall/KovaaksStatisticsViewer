#include <gtest/gtest.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QVariantMap>
#include <memory>

#include "qml_registration.h"

namespace {
    TEST(BenchmarkTableContract, ShellInstantiates) {
        ksv::declare_metatypes();
        QQmlEngine engine;
        QQmlComponent manager(&engine);
        manager.setData("import QtQml\nQtObject {}", QUrl());
        std::unique_ptr<QObject> managerObject(manager.create());
        ASSERT_TRUE(managerObject) << manager.errorString().toStdString();

        QQmlComponent component(&engine);
        component.loadFromModule("KovaaksStatsViewer", "BenchmarkEditorTable");
        ASSERT_FALSE(component.isError()) << component.errorString().toStdString();
        std::unique_ptr<QObject> table(component.createWithInitialProperties(
            QVariantMap{{"manager", QVariant::fromValue(managerObject.get())}}));
        ASSERT_TRUE(table) << component.errorString().toStdString();

        const QMetaObject *meta = table->metaObject();
        EXPECT_GE(meta->indexOfMethod("commitActiveEdit()"), 0);
        EXPECT_GE(meta->indexOfMethod("focusCell(QVariant)"), 0);
    }
}
