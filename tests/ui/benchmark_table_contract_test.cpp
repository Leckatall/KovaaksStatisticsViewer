#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "benchmarks/benchmark_editor.h"
#include "benchmarks/benchmark_validation.h"
#include "presentation/benchmark_cell_input.h"
#include "presentation/benchmark_clipboard_adapter.h"
#include "presentation/benchmark_clipboard_matrix.h"
#include "presentation/benchmark_manager_vm.h"
#include "presentation/benchmark_table_model.h"
#include "fake_benchmark_manager_use_case.h"
#include "qml_registration.h"

using namespace ksv::domain;
using namespace ksv::presentation;
using namespace ksv::tests_support;

namespace {
    // Structural only: these prove the agreed seams exist with their intended shapes. Behaviour
    // is asserted by the D*/P*/Q* suites.
    static_assert(std::is_same_v<decltype(BenchmarkIssue::tierId), std::optional<TierId>>);
    static_assert(std::is_base_of_v<QAbstractTableModel, BenchmarkTableModel>);
    static_assert(std::is_same_v<decltype(BenchmarkEditResult::createdEntries), std::vector<ScenarioEntryId>>);

    TEST(BenchmarkTableContract, CompilesAgreedContracts) {
        const BenchmarkIssue legacy{BenchmarkIssueCode::MissingThreshold, ScenarioEntryId{"e"}};
        EXPECT_FALSE(legacy.tierId.has_value());
        [[maybe_unused]] const BenchmarkIssueCode names[] = {
            BenchmarkIssueCode::MissingTierName, BenchmarkIssueCode::MissingScenarioName,
            BenchmarkIssueCode::MissingGroupName};

        Benchmark benchmark;
        BenchmarkEditor editor{benchmark, [] { return std::string("id"); }};
        [[maybe_unused]] BenchmarkEditResult r;
        r = editor.appendUnnamedScenarios(std::size_t{2});
        r = editor.assignScenarios(std::vector<ScenarioEntryId>{}, EditorGroupTarget{});
        r = editor.addSubcategory(GroupId{"c"}, "sub", DirectScenarioRelocation{RelocateDirectToUncategorized{}});
        r = editor.addSubcategory(GroupId{"c"}, "sub",
                                  DirectScenarioRelocation{RelocateDirectToNewSubcategory{}});
        r = editor.reorderSubcategory(GroupId{"s"}, std::size_t{0});
        r = editor.reorderScenario(ScenarioEntryId{"e"}, std::size_t{0});

        const BenchmarkCellKey key{ScenarioEntryId{"e"}, TierId{"t"}};
        [[maybe_unused]] const BenchmarkCellRecord record{QStringLiteral("12oops"), CellParseOutcome::Invalid};
        [[maybe_unused]] const CellParseResult parsed = BenchmarkCellInput::parse(QStringLiteral("1"), QLocale::c());
        [[maybe_unused]] const QString display = BenchmarkCellInput::displayText(1.0, QLocale::c());
        [[maybe_unused]] const QString edit = BenchmarkCellInput::editText(1.0, QLocale::c());
        [[maybe_unused]] const CellParseOutcome outcomes[] = {
            CellParseOutcome::Missing, CellParseOutcome::Finite, CellParseOutcome::Invalid,
            CellParseOutcome::NonFinite};
        EXPECT_EQ(key, key);

        const BenchmarkClipboardMatrix matrix = BenchmarkClipboardMatrix::parse(QStringLiteral("a\tb"));
        [[maybe_unused]] const std::vector<std::vector<QString>> &rows = matrix.rows;
        [[maybe_unused]] const int width = matrix.width();

        BenchmarkClipboardAdapter adapter;
        [[maybe_unused]] const QString clipboardText = adapter.text();

        BenchmarkTableModel model;
        const auto roles = model.roleNames();
        for (const int role: {BenchmarkTableModel::EntryIdRole, BenchmarkTableModel::TierIdRole,
                              BenchmarkTableModel::ColumnKindRole, BenchmarkTableModel::DisplayTextRole,
                              BenchmarkTableModel::EditTextRole, BenchmarkTableModel::HasValueRole,
                              BenchmarkTableModel::InputStateRole, BenchmarkTableModel::IssuesRole,
                              BenchmarkTableModel::CategoryIdRole, BenchmarkTableModel::CategoryNameRole,
                              BenchmarkTableModel::CategoryColorRole,
                              BenchmarkTableModel::CategorySpanStartRole,
                              BenchmarkTableModel::CategorySpanLengthRole,
                              BenchmarkTableModel::SubcategoryIdRole,
                              BenchmarkTableModel::SubcategoryNameRole,
                              BenchmarkTableModel::SubcategoryColorRole,
                              BenchmarkTableModel::SubcategorySpanStartRole,
                              BenchmarkTableModel::SubcategorySpanLengthRole,
                              BenchmarkTableModel::MappingStateRole,
                              BenchmarkTableModel::MappingCandidatesRole})
            EXPECT_TRUE(roles.contains(role)) << role;
        [[maybe_unused]] const BenchmarkColumnKind kinds[] = {
            BenchmarkColumnKind::Category, BenchmarkColumnKind::Subcategory, BenchmarkColumnKind::Scenario,
            BenchmarkColumnKind::Threshold};

        BenchmarkManagerViewModel vm{std::make_shared<FakeBenchmarkManagerUseCase>()};
        [[maybe_unused]] QAbstractItemModel *table = vm.tableModel();
        [[maybe_unused]] QVariantMap m;
        m = vm.editThresholdText(QStringLiteral("e"), QStringLiteral("t"), QStringLiteral("1"));
        m = vm.pasteText(QVariantMap{{"kind", "threshold"}}, QStringLiteral("1"));
        m = vm.assignScenarios(QStringList{}, QString());
        m = vm.addSubcategoryRelocating(QStringLiteral("c"), QStringLiteral("s"), QStringLiteral("uncategorized"));
        m = vm.reorderSubcategory(QStringLiteral("s"), 0);
        m = vm.reorderScenario(QStringLiteral("e"), 0);
        m = vm.undo();
        [[maybe_unused]] const bool canUndo = vm.canUndo();
        vm.setSelectedEntryIds(QStringList{});
        [[maybe_unused]] const QStringList selected = vm.selectedEntryIds();
        vm.setCurrentCell(QVariantMap{});
        [[maybe_unused]] const QVariantMap current = vm.currentCell();
        [[maybe_unused]] const QVariantList normalization = vm.normalizationIssues();
    }

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
