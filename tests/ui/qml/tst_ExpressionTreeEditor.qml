import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase

    function findByObjectName(root, name) {
        if (!root)
            return null;
        if (root.objectName === name)
            return root;
        for (const child of root.children || []) {
            const found = findByObjectName(child, name);
            if (found)
                return found;
        }
        return null;
    }
    function findByText(root, text) {
        if (!root)
            return null;
        if (root.text === text)
            return root;
        for (const child of root.children || []) {
            const found = findByText(child, text);
            if (found)
                return found;
        }
        return null;
    }
    function makeFakeModel(overrides) {
        return createTemporaryObject(fakeModelComponent, testCase, overrides || {});
    }
    function test_averageAcrossRunsRootRendersSelectionKindComboAndCountOrPercentSpinBox() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "averageAcrossRuns",
                selection: {
                    kind: "recentRuns",
                    count: 6
                },
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(findByObjectName(editor, "selectionKindComboBox_node-1") !== null);
        compare(findByObjectName(editor, "selectionValueSpinBox_node-1").value, 6);
    }
    function test_binaryRootRendersOperatorComboBoxAndTwoChildSlots() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "add",
                left: {},
                right: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "operatorComboBox_node-1") !== null);
        verify(findByObjectName(editor, "kindChooser_node-1_left") !== null);
        verify(findByObjectName(editor, "kindChooser_node-1_right") !== null);
    }
    function test_changingConstantSpinBoxCallsUpdateFieldWithValueField() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "constant",
                value: 1
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const spin = findByObjectName(editor, "constantSpinBox_node-1");
        spin.value = 8;
        spin.valueModified();
        compare(model.updateCalls[0], {
            id: "node-1",
            field: "value",
            value: 8
        });
    }
    function test_changingCountSpinBoxCallsUpdateFieldWithCountFieldWhenSelectionIsRecentRuns() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "averageAcrossRuns",
                selection: {
                    kind: "recentRuns",
                    count: 6
                },
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const spin = findByObjectName(editor, "selectionValueSpinBox_node-1");
        spin.value = 9;
        spin.valueModified();
        compare(model.updateCalls[0].field, "count");
    }
    function test_changingOperatorComboBoxCallsChangeBinaryOperator() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "add",
                left: {},
                right: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const combo = findByObjectName(editor, "operatorComboBox_node-1");
        combo.currentIndex = 3;
        combo.activated(3);
        compare(model.binaryCalls[0], {
            id: "node-1",
            kind: "divide"
        });
        model.changeBinaryOperator("node-1", "subtract");
        wait(20);
        compare(combo.currentText, "subtract");
    }
    function test_changingPercentSpinBoxCallsUpdateFieldWithPercentFieldWhenSelectionIsTopPercentile() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "averageAcrossRuns",
                selection: {
                    kind: "topPercentile",
                    percent: 15
                },
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const spin = findByObjectName(editor, "selectionValueSpinBox_node-1");
        spin.value = 20;
        spin.valueModified();
        compare(model.updateCalls[0].field, "percent");
    }
    function test_changingSelectionKindComboCallsChangeSelectionKind() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "averageAcrossRuns",
                selection: {
                    kind: "recentRuns",
                    count: 6
                },
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const combo = findByObjectName(editor, "selectionKindComboBox_node-1");
        combo.currentIndex = 1;
        combo.activated(1);
        compare(model.selectionKindCalls[0], {
            id: "node-1",
            kind: "topPercentile"
        });
        compare(findByObjectName(editor, "selectionValueSpinBox_node-1").value, 10);
    }
    function test_changingWindowSpinBoxCallsUpdateFieldWithWindowField() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const spin = findByObjectName(editor, "windowSpinBox_node-1");
        spin.value = 15;
        spin.valueModified();
        compare(model.updateCalls[0], {
            id: "node-1",
            field: "window",
            value: 15
        });
    }
    function test_choosingKindAndClickingAddCallsReplaceChildWithEmptyParentIdAndRootSlot() {
        const model = makeFakeModel();
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        const chooser = findByObjectName(editor, "kindChooser__root");
        chooser.currentIndex = 1;
        mouseClick(findByObjectName(editor, "addNodeButton__root"));
        compare(model.replaceCalls[0], {
            parentId: "",
            slot: "root",
            kind: "constant"
        });
    }
    function test_clickingFoldedChipCallsSelectWithChildNodeId() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {
                    id: "node-2",
                    kind: "primitive",
                    metric: "hits"
                }
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        const label = findByText(editor, "hits");
        const point = label.mapToItem(editor, 1, 1);
        mouseClick(editor, point.x, point.y);
        compare(model.selectCalls[0], "node-2");
    }
    function test_clickingWrapCallsWrapSelectedWithChosenKind() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        const combo = findByObjectName(editor, "wrapKindComboBox");
        combo.currentIndex = 1;
        mouseClick(findByObjectName(editor, "wrapNodeButton"));
        compare(model.wrapCalls[0], "rollingMean");
    }
    function test_constantRootRendersSpinBoxBoundToNodeValue() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "constant",
                value: 42
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        compare(findByObjectName(editor, "constantSpinBox_node-1").value, 42);
    }
    function test_deleteButtonCallsDeleteNodeWithFocusedNodeId() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        mouseClick(findByObjectName(editor, "deleteNodeButton_node-1"));
        compare(model.deleteCalls[0], "node-1");
    }
    function test_emptyChildSlotRendersKindChooserForThatSlot() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "add",
                left: {},
                right: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(findByObjectName(editor, "kindChooser_node-1_left") !== null);
    }
    function test_emptyModelShowsRootKindChooser() {
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: makeFakeModel()
        });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "kindChooser__root") !== null);
    }
    function test_modelWithPopulatedRootAndAnExistingSelectionDoesNotCallSelectAgain() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            },
            selectedNodeId: "node-1"
        });
        createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        wait(20);
        compare(model.selectCalls.length, 0);
    }
    function test_modelWithPopulatedRootAndNoSelectionAutoSelectsRootOnComponentCompleted() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            }
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: makeFakeModel()
        });
        editor.model = model;
        verify(waitForRendering(editor));
        wait(20);
        compare(model.selectCalls, ["node-1"]);
        verify(findByObjectName(editor, "metricComboBox_node-1") !== null);
    }
    function test_nestedSelectionRendersCollapsedAncestorCardAboveFocusedCard() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {
                    id: "node-2",
                    kind: "primitive",
                    metric: "hits"
                }
            },
            selectedNodeId: "node-2"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        wait(20);
        verify(findByObjectName(editor, "windowSpinBox_node-1") === null);
        verify(findByObjectName(editor, "metricComboBox_node-2") !== null);
    }
    function test_populatedChildSlotRendersFoldedChipShowingDescribeText() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {
                    id: "node-2",
                    kind: "primitive",
                    metric: "hits"
                }
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        verify(findByText(editor, "hits") !== null);
    }
    function test_primitiveMetricWritesThroughTheModel() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            }
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: makeFakeModel()
        });
        editor.model = model;
        verify(waitForRendering(editor));
        wait(20);
        const combo = findByObjectName(editor, "metricComboBox_node-1");
        verify(combo !== null);
        combo.activated(1);
        compare(model.updateCalls[0].field, "metric");
    }
    function test_primitiveRootRendersMetricComboBoxBoundToNodeMetric() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "kills"
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        compare(findByObjectName(editor, "metricComboBox_node-1").currentText, "kills");
    }
    function test_projectRateToFinalRootRendersNoFieldEditor() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "projectRateToFinal",
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(findByObjectName(editor, "metricComboBox_node-1") === null);
        verify(findByObjectName(editor, "windowSpinBox_node-1") === null);
    }
    function test_projectedFinalValueRootRendersNoFieldEditor() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "projectedFinalValue",
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(findByObjectName(editor, "metricComboBox_node-1") === null);
        verify(findByObjectName(editor, "windowSpinBox_node-1") === null);
    }
    function test_rebindingToADifferentModelWithPopulatedRootAndNoSelectionAlsoAutoSelectsRoot() {
        const first = makeFakeModel();
        const second = makeFakeModel({
            root: {
                id: "node-2",
                kind: "primitive",
                metric: "hits"
            }
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: first
        });
        editor.model = second;
        wait(20);
        compare(second.selectCalls, ["node-2"]);
    }
    function test_rollingMeanRootRendersWindowSpinBox() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 12,
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        compare(findByObjectName(editor, "windowSpinBox_node-1").value, 12);
    }
    function test_runningSumRootRendersNoFieldEditor() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "runningSum",
                input: {}
            },
            selectedNodeId: "node-1"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(findByObjectName(editor, "metricComboBox_node-1") === null);
        verify(findByObjectName(editor, "windowSpinBox_node-1") === null);
    }
    function test_selectingAnAncestorCardHeaderCallsSelectWithThatNodeId() {
        // TODO (2026/08/21): This test fails non-deterministically.
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {
                    id: "node-2",
                    kind: "primitive",
                    metric: "hits"
                }
            },
            selectedNodeId: "node-2"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        wait(20);
        const label = findByText(editor, "Rolling mean");
        const point = label.mapToItem(editor, 1, 1);
        mouseClick(editor, point.x, point.y);
        compare(model.selectCalls[0], "node-1");
        wait(20);
        verify(findByObjectName(editor, "metricComboBox_node-2") === null);
    }
    function test_swappingInAPopulatedPrimitiveModelLogsNoWarnings() {
        failOnWarning(/.?/);
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: makeFakeModel()
        });
        editor.model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "primitive",
                metric: "score"
            },
            selectedNodeId: "node-1"
        });
        verify(waitForRendering(editor));
        wait(20);
        verify(findByObjectName(editor, "metricComboBox_node-1") !== null);
    }
    function test_switchingBetweenTwoDeepSelectionsUpdatesAlreadyMountedCollapsedCards() {
        const model = makeFakeModel({
            root: {
                id: "node-1",
                kind: "rollingMean",
                window: 10,
                input: {
                    id: "node-2",
                    kind: "add",
                    left: {
                        id: "node-3",
                        kind: "primitive",
                        metric: "hits"
                    },
                    right: {
                        id: "node-4",
                        kind: "primitive",
                        metric: "shots"
                    }
                }
            },
            selectedNodeId: "node-3"
        });
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: model
        });
        verify(waitForRendering(editor));
        wait(20);
        verify(findByObjectName(editor, "metricComboBox_node-3") !== null);
        model.select("node-4");
        wait(20);
        verify(findByObjectName(editor, "metricComboBox_node-3") === null);
        verify(findByObjectName(editor, "metricComboBox_node-4") !== null);
    }
    function test_wrapControlVisibleOnlyWhenANodeIsSelected() {
        const empty = makeFakeModel();
        const editor = createTemporaryObject(editorComponent, testCase, {
            model: empty
        });
        verify(findByObjectName(editor, "wrapNodeButton").visible === false);
        empty.replaceChild("", "root", "primitive");
        wait(20);
        verify(findByObjectName(editor, "wrapNodeButton").visible);
    }

    height: 500
    name: "ExpressionTreeEditorTest"
    visible: true
    when: windowShown
    width: 600

    Component {
        id: editorComponent

        ExpressionTreeEditor {
            height: 400
            width: 500
        }
    }
    Component {
        id: fakeModelComponent

        QtObject {
            property var binaryCalls: []
            property var deleteCalls: []
            property int nextId: 1
            property var nodeKinds: ["primitive", "constant", "add", "subtract", "multiply", "divide", "runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns"]
            property var primitiveMetrics: ["score", "shots", "hits", "kills", "dmg"]
            property var replaceCalls: []
            property var root: ({})
            property var selectCalls: []
            property string selectedNodeId: ""
            property var selectionKindCalls: []
            property int treeRevision: 0
            property var updateCalls: []
            property var wrapCalls: []

            function ancestorChain(id) {
                return pathTo(id, root, []) || [];
            }
            function changeBinaryOperator(id, kind) {
                binaryCalls.push({
                    id: id,
                    kind: kind
                });
                const location = locationFor(id);
                if (!location || !isBinary(kind))
                    return;
                location.node.kind = kind;
                touch();
            }
            function changeSelectionKind(id, kind) {
                selectionKindCalls.push({
                    id: id,
                    kind: kind
                });
                const location = locationFor(id);
                if (!location || location.node.kind !== "averageAcrossRuns")
                    return;
                location.node.selection = kind === "recentRuns" ? {
                    kind: kind,
                    count: 5
                } : {
                    kind: kind,
                    percent: 10
                };
                touch();
            }
            function childSlotsFor(kind) {
                return isBinary(kind) ? ["left", "right"] : ["runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns"].indexOf(kind) >= 0 ? ["input"] : [];
            }
            function deleteNode(id) {
                deleteCalls.push(id);
                const location = locationFor(id);
                if (!location)
                    return;
                if (location.parent) {
                    location.parent[location.slot] = {};
                    selectedNodeId = location.parent.id;
                } else {
                    root = {};
                    selectedNodeId = "";
                }
                touch();
            }
            function describe(node) {
                return node.kind === "primitive" ? node.metric : node.kind;
            }
            function findLocation(id, node, parent, slot) {
                if (!node || !node.kind)
                    return null;
                if (node.id === id)
                    return {
                        node: node,
                        parent: parent,
                        slot: slot
                    };
                if (isBinary(node.kind))
                    return findLocation(id, node.left, node, "left") || findLocation(id, node.right, node, "right");
                return findLocation(id, node.input, node, "input");
            }
            function isBinary(kind) {
                return ["add", "subtract", "multiply", "divide"].indexOf(kind) >= 0;
            }
            function locationFor(id) {
                return findLocation(id, root, null, "root");
            }
            function makeNode(kind) {
                const node = {
                    id: "node-" + nextId++,
                    kind: kind
                };
                if (kind === "primitive")
                    node.metric = "score";
                else if (kind === "constant")
                    node.value = 0;
                else if (isBinary(kind)) {
                    node.left = {};
                    node.right = {};
                } else {
                    node.input = {};
                    if (kind === "rollingMean")
                        node.window = 10;
                    if (kind === "averageAcrossRuns")
                        node.selection = {
                            kind: "recentRuns",
                            count: 5
                        };
                }
                return node;
            }
            function pathTo(id, node, path) {
                if (!node || !node.kind)
                    return null;
                const extended = path.concat([node]);
                if (node.id === id)
                    return extended;
                if (isBinary(node.kind))
                    return pathTo(id, node.left, extended) || pathTo(id, node.right, extended);
                return pathTo(id, node.input, extended);
            }
            function replaceChild(parentId, slot, kind) {
                replaceCalls.push({
                    parentId: parentId,
                    slot: slot,
                    kind: kind
                });
                const replacement = makeNode(kind);
                if (slot === "root")
                    root = replacement;
                else {
                    const parent = locationFor(parentId);
                    if (!parent)
                        return;
                    parent.node[slot] = replacement;
                }
                selectedNodeId = replacement.id;
                touch();
            }
            function select(id) {
                selectCalls.push(id);
                selectedNodeId = id;
            }
            function touch() {
                treeRevision++;
            }
            function updateField(id, field, value) {
                updateCalls.push({
                    id: id,
                    field: field,
                    value: value
                });
                const location = locationFor(id);
                if (!location)
                    return;
                if (location.node.kind === "averageAcrossRuns" && (field === "count" || field === "percent"))
                    location.node.selection[field] = value;
                else
                    location.node[field] = value;
                touch();
            }
            function wrapSelected(kind) {
                wrapCalls.push(kind);
                const location = locationFor(selectedNodeId);
                if (!location)
                    return;
                const wrapper = makeNode(kind);
                if (isBinary(kind))
                    wrapper.left = location.node;
                else
                    wrapper.input = location.node;
                if (location.parent)
                    location.parent[location.slot] = wrapper;
                else
                    root = wrapper;
                selectedNodeId = wrapper.id;
                touch();
            }
        }
    }
}
