#ifndef KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
#define KOVAAKSSTATSVIEWER_QML_REGISTRATION_H

#include <QtQml>

#include <ui/components/graph_canvas.h>
#include <presentation/graph_vm.h>
#include <presentation/completion_history_vm.h>
#include <presentation/playtime_graph_vm.h>
#include <presentation/editable_expression_node.h>
#include <presentation/benchmark_manager_vm.h>
#include <presentation/benchmark_tree_node.h>
#include <presentation/benchmark_tracking_vm.h>
#include <presentation/benchmark_history_vm.h>
#include <presentation/benchmark_breakdown_model.h>

namespace ksv {
    // Referencing these types keeps the linker from dropping the static QML
    // module's auto-registration objects (ksv_ui is built NO_PLUGIN). Called
    // from every executable that loads the QML module.
    inline void declare_metatypes() {
        qmlRegisterUncreatableType<presentation::GraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "GraphViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::PlaytimeGraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "PlaytimeGraphViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::CompletionHistoryViewModel>(
            "KovaaksStatsViewer", 1, 0, "CompletionHistoryViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::EditableExpressionNode>(
            "KovaaksStatsViewer", 1, 0, "EditableExpressionNode", "Abstract base, use a concrete node type");
        qmlRegisterType<presentation::EditablePrimitiveNode>(
            "KovaaksStatsViewer", 1, 0, "EditablePrimitiveNode");
        qmlRegisterType<presentation::EditableConstantNode>(
            "KovaaksStatsViewer", 1, 0, "EditableConstantNode");
        qmlRegisterType<presentation::EditableBinaryOpNode>(
            "KovaaksStatsViewer", 1, 0, "EditableBinaryOpNode");
        qmlRegisterType<presentation::EditableUnaryOpNode>(
            "KovaaksStatsViewer", 1, 0, "EditableUnaryOpNode");
        qmlRegisterType<presentation::EditableRollingMeanNode>(
            "KovaaksStatsViewer", 1, 0, "EditableRollingMeanNode");
        qmlRegisterType<presentation::EditableAverageAcrossRunsNode>(
            "KovaaksStatsViewer", 1, 0, "EditableAverageAcrossRunsNode");
        qmlRegisterUncreatableType<presentation::BenchmarkManagerViewModel>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkManagerViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::BenchmarkTreeNode>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkTreeNode", "Abstract base, built by BenchmarkManagerViewModel");
        qmlRegisterType<presentation::BenchmarkGroupNode>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkGroupNode");
        qmlRegisterType<presentation::BenchmarkScenarioNode>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkScenarioNode");
        qmlRegisterType<ui::GraphCanvas>(
            "KovaaksStatsViewer", 1, 0, "GraphCanvas");
        qmlRegisterUncreatableType<presentation::BenchmarkTrackingViewModel>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkTrackingViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::BenchmarkHistoryViewModel>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkHistoryViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::BenchmarkBreakdownModel>(
            "KovaaksStatsViewer", 1, 0, "BenchmarkBreakdownModel", "Created in C++");
    }
}

#endif //KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
