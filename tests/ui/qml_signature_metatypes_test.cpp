//
// Every type in a QML-facing meta signature must resolve without declare_metatypes(). A type moc only saw
// forward-declared is recorded by name and resolved at call time, so it works only while some unrelated
// registration happens to exist — remove that and QML fails with "Unknown method return type". This runs
// in its own executable because ui_tests calls declare_metatypes(), which would mask exactly that.
//

#include <gtest/gtest.h>

#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QStringList>

#include <ui/components/graph_canvas.h>
#include "presentation/app/session_vm.h"
#include "presentation/app/settings_vm.h"
#include "presentation/benchmarks/manager/benchmark_clipboard_adapter.h"
#include "presentation/benchmarks/manager/benchmark_manager_vm.h"
#include "presentation/benchmarks/manager/benchmark_table_model.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/expression/editable_expression_node.h"
#include "presentation/expression/series_expression_editor_model.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"
#include "presentation/graphs/completion_history_vm.h"
#include "presentation/graphs/graph_vm.h"
#include "presentation/graphs/playtime_graph_vm.h"
#include "presentation/scenarios/run_list_model.h"
#include "presentation/scenarios/scenario_browser_vm.h"
#include "presentation/scenarios/scenario_list_model.h"

using namespace ksv::presentation;

namespace {
    const QMetaObject *const kQmlFacingTypes[] = {
        &ksv::ui::GraphCanvas::staticMetaObject,
        &SessionViewModel::staticMetaObject,
        &SettingsViewModel::staticMetaObject,
        &BenchmarkClipboardAdapter::staticMetaObject,
        &BenchmarkManagerViewModel::staticMetaObject,
        &BenchmarkTableModel::staticMetaObject,
        &BenchmarkHistoryViewModel::staticMetaObject,
        &BenchmarkProgressGridModel::staticMetaObject,
        &BenchmarkTrackingViewModel::staticMetaObject,
        &EditableExpressionNode::staticMetaObject,
        &EditablePrimitiveNode::staticMetaObject,
        &EditableFinalValueNode::staticMetaObject,
        &EditableConstantNode::staticMetaObject,
        &EditableBinaryOpNode::staticMetaObject,
        &EditableUnaryOpNode::staticMetaObject,
        &EditableRollingMeanNode::staticMetaObject,
        &EditableAverageAcrossRunsNode::staticMetaObject,
        &SeriesExpressionEditorModel::staticMetaObject,
        &GraphViewModelBase::staticMetaObject,
        &SeriesModel::staticMetaObject,
        &CompletionHistoryViewModel::staticMetaObject,
        &GraphViewModel::staticMetaObject,
        &PlaytimeGraphViewModel::staticMetaObject,
        &RunListModel::staticMetaObject,
        &ScenarioBrowserViewModel::staticMetaObject,
        &ScenarioListModel::staticMetaObject,
    };

    QStringList unresolvedSignatureTypes(const QMetaObject &meta) {
        QStringList unresolved;
        const QString owner = QString::fromLatin1(meta.className());
        for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
            const QMetaProperty property = meta.property(i);
            if (!property.metaType().isValid())
                unresolved << QStringLiteral("%1::%2 property type %3").arg(owner, property.name(), property.typeName());
        }
        for (int i = meta.methodOffset(); i < meta.methodCount(); ++i) {
            const QMetaMethod method = meta.method(i);
            const QString signature = QString::fromLatin1(method.methodSignature());
            if (!method.returnMetaType().isValid())
                unresolved << QStringLiteral("%1::%2 return type %3").arg(owner, signature, method.typeName());
            for (int p = 0; p < method.parameterCount(); ++p) {
                if (!method.parameterMetaType(p).isValid())
                    unresolved << QStringLiteral("%1::%2 parameter %3 type %4")
                                      .arg(owner, signature).arg(p).arg(QString::fromLatin1(method.parameterTypeName(p)));
            }
        }
        return unresolved;
    }

    TEST(QmlSignatureMetatypesTest, EveryQmlFacingSignatureTypeResolvesWithoutRuntimeRegistration) {
        QStringList unresolved;
        for (const QMetaObject *meta: kQmlFacingTypes) unresolved << unresolvedSignatureTypes(*meta);
        EXPECT_TRUE(unresolved.isEmpty()) << unresolved.join(QLatin1Char('\n')).toStdString();
    }
}
