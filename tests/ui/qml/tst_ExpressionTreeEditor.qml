import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml"

TestCase {
    id: testCase

    function findByObjectName(root, name) {
        if (!root) return null;
        if (root.objectName === name) return root;
        for (const child of root.children || []) {
            const found = findByObjectName(child, name);
            if (found) return found;
        }
        return null;
    }
    function findByText(root, text) {
        if (!root) return null;
        if (root.text === text) return root;
        for (const child of root.children || []) {
            const found = findByText(child, text);
            if (found) return found;
        }
        return null;
    }
    function makeFakeModel(overrides) {
        return createTemporaryObject(fakeModelComponent, testCase, overrides || {});
    }
    function makePrimitive(metric) {
        return createTemporaryObject(primitiveNodeComponent, testCase, { metric: metric || "score" });
    }
    function makeBinary(kind, left, right) {
        return createTemporaryObject(binaryOpNodeComponent, testCase, { operatorKind: kind, left: left || null, right: right || null });
    }
    function makeRollingMean(window, input) {
        return createTemporaryObject(rollingMeanNodeComponent, testCase, { window: window || 10, input: input || null });
    }
    function makeUnary(kind, input) {
        return createTemporaryObject(unaryOpNodeComponent, testCase, { operatorKind: kind, input: input || null });
    }
    function makeRollingMeanWithPrimitive(window, metric) {
        return Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableRollingMeanNode { window: ' + window
            + '; input: EditablePrimitiveNode { metric: "' + metric + '" } }', testCase, "rollingMeanWithPrimitive");
    }

    function test_emptyModelShowsRootKindChooser() {
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel() });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "kindChooser_root") !== null);
    }
    function test_choosingRootKindCallsReplaceChild() {
        const model = makeFakeModel();
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        const chooser = findByObjectName(editor, "kindChooser_root");
        chooser.currentIndex = 1;
        findByObjectName(editor, "addNodeButton_root").clicked();
        compare(model.replaceCalls[0].parent, null);
        compare(model.replaceCalls[0].slot, "root");
        compare(model.replaceCalls[0].kind, "constant");
    }
    function test_primitiveRootRendersMetricComboBoxAndWritesNode() {
        const node = makePrimitive("kills");
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        verify(waitForRendering(editor));
        const combo = findByObjectName(editor, "metricComboBox");
        compare(combo.currentText, "kills");
        combo.currentIndex = 1;
        combo.activated(1);
        compare(node.metric, "shots");
    }
    function test_constantRootRendersSpinBoxAndWritesNode() {
        const node = createTemporaryObject(constantNodeComponent, testCase, { value: 1 });
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        const spin = findByObjectName(editor, "constantSpinBox");
        compare(spin.value, 1);
        spin.value = 8;
        spin.valueModified();
        compare(node.value, 8);
    }
    function test_binaryRootRendersOperatorAndChildSlots() {
        const node = makeBinary("add");
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "operatorComboBox") !== null);
        verify(findByObjectName(editor, "kindChooser_left") !== null);
        verify(findByObjectName(editor, "kindChooser_right") !== null);
    }
    function test_changingOperatorWritesNodeKind() {
        const node = makeBinary("add");
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        const combo = findByObjectName(editor, "operatorComboBox");
        combo.currentIndex = 3;
        combo.activated(3);
        compare(node.kind, "divide");
    }
    function test_childKindChooserCallsReplaceChildWithParentAndSlot() {
        const node = makeBinary("add");
        const model = makeFakeModel({ root: node, selected: node });
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        findByObjectName(editor, "addNodeButton_left").clicked();
        compare(model.replaceCalls[0].parent, node);
        compare(model.replaceCalls[0].slot, "left");
        compare(model.replaceCalls[0].kind, "primitive");
    }
    function test_rollingMeanRootRendersWindowAndWritesNode() {
        const node = makeRollingMean(12);
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        const spin = findByObjectName(editor, "windowSpinBox");
        compare(spin.value, 12);
        spin.value = 15;
        spin.valueModified();
        compare(node.window, 15);
    }
    function test_averageAcrossRunsWritesSelectionAndSelectionValue() {
        const node = createTemporaryObject(averageNodeComponent, testCase, { count: 6 });
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        const combo = findByObjectName(editor, "selectionKindComboBox");
        const spin = findByObjectName(editor, "selectionValueSpinBox");
        compare(spin.value, 6);
        combo.currentIndex = 1;
        combo.activated(1);
        compare(node.selectionKind, "topPercentile");
        compare(spin.value, 10);
        spin.value = 20;
        spin.valueModified();
        compare(node.percent, 20);
    }
    function test_unaryRootRendersNoFieldEditor() {
        const node = makeUnary("projectRateToFinal");
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel({ root: node, selected: node }) });
        verify(findByObjectName(editor, "metricComboBox") === null);
        verify(findByObjectName(editor, "windowSpinBox") === null);
    }
    function test_deleteAndWrapDispatchStructuralOperations() {
        const node = makePrimitive();
        const model = makeFakeModel({ root: node, selected: node });
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        findByObjectName(editor, "deleteNodeButton").clicked();
        compare(model.deleteCalls[0], node);
        findByObjectName(editor, "wrapKindComboBox").currentIndex = 1;
        findByObjectName(editor, "wrapNodeButton").clicked();
        compare(model.wrapCalls[0], "rollingMean");
    }
    function test_collapsedAncestorRendersFoldedChipAndSelectsChild() {
        const root = makeRollingMeanWithPrimitive(9, "hits");
        const leaf = root.input;
        const model = makeFakeModel({ root: root, selected: root, parentOverrides: new Map([[leaf, root]]) });
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        verify(waitForRendering(editor));
        const chip = findByObjectName(editor, "foldedChip_0_input");
        verify(chip !== null);
        chip.selected();
        compare(model.selected, leaf);
        verify(findByObjectName(editor, "pathCard_0") !== null);
        verify(findByObjectName(editor, "pathCard_1") !== null);
    }
    function test_selectingAncestorCardChangesFocusedEditor() {
        const root = makeRollingMeanWithPrimitive(9, "hits");
        const leaf = root.input;
        const model = makeFakeModel({ root: root, selected: leaf, parentOverrides: new Map([[leaf, root]]) });
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "metricComboBox") !== null);
        findByObjectName(editor, "pathCard_0").selected();
        verify(findByObjectName(editor, "windowSpinBox") !== null);
    }
    function test_modelWithRootAndNoSelectionAutoSelectsRoot() {
        const node = makePrimitive();
        const model = makeFakeModel({ root: node });
        createTemporaryObject(editorComponent, testCase, { model: model });
        tryCompare(model, "selected", node);
    }
    function test_swappingInAPopulatedPrimitiveModelLogsNoWarnings() {
        failOnWarning(/.?/);
        const editor = createTemporaryObject(editorComponent, testCase, { model: makeFakeModel() });
        const node = makePrimitive();
        editor.model = makeFakeModel({ root: node, selected: node });
        verify(waitForRendering(editor));
        verify(findByObjectName(editor, "metricComboBox") !== null);
    }
    function test_editingDeeplyNestedSubtractOfProjectRateToFinalOfRollingMeanDoesNotThrowRangeError() {
        const root = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableBinaryOpNode { operatorKind: "subtract"; '
            + 'left: EditableUnaryOpNode { operatorKind: "projectRateToFinal"; '
            + 'input: EditableRollingMeanNode { window: 10; input: EditablePrimitiveNode { metric: "score" } } } '
            + 'right: EditableUnaryOpNode { operatorKind: "projectRateToFinal"; '
            + 'input: EditableRollingMeanNode { window: 10; input: EditablePrimitiveNode { metric: "score" } } } }',
            testCase, "deepExpression");
        const leftProject = root.left;
        const leftRolling = leftProject.input;
        const leftLeaf = leftRolling.input;
        const rightProject = root.right;
        const rightRolling = rightProject.input;
        const rightLeaf = rightRolling.input;
        const parents = new Map([[leftProject, root], [leftRolling, leftProject], [leftLeaf, leftRolling], [rightProject, root], [rightRolling, rightProject], [rightLeaf, rightRolling]]);
        const model = makeFakeModel({ root: root, selected: root, parentOverrides: parents });

        failOnWarning(/RangeError: Maximum call stack size exceeded/);
        const editor = createTemporaryObject(editorComponent, testCase, { model: model });
        verify(waitForRendering(editor));

        function selectChip(cardIndex, slot) {
            const chip = findByObjectName(editor, "foldedChip_" + cardIndex + "_" + slot);
            verify(chip !== null);
            chip.selected();
            wait(20);
        }
        selectChip(0, "left");
        selectChip(1, "input");
        selectChip(2, "input");
        findByObjectName(editor, "pathCard_0").selected();
        wait(20);
        selectChip(0, "right");
        selectChip(1, "input");
        selectChip(2, "input");
    }

    height: 500
    name: "ExpressionTreeEditorTest"
    visible: true
    when: windowShown
    width: 600

    Component { id: editorComponent; ExpressionTreeEditor { height: 400; width: 500 } }
    Component { id: primitiveNodeComponent; EditablePrimitiveNode {} }
    Component { id: constantNodeComponent; EditableConstantNode {} }
    Component { id: binaryOpNodeComponent; EditableBinaryOpNode {} }
    Component { id: unaryOpNodeComponent; EditableUnaryOpNode {} }
    Component { id: rollingMeanNodeComponent; EditableRollingMeanNode {} }
    Component { id: averageNodeComponent; EditableAverageAcrossRunsNode {} }

    Component {
        id: fakeModelComponent

        QtObject {
            property var root: null
            property var selected: null
            property var nodeKinds: ["primitive", "constant", "add", "subtract", "multiply", "divide", "runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns"]
            property var primitiveMetrics: ["score", "shots", "hits", "kills", "dmg"]
            property var deleteCalls: []
            property var wrapCalls: []
            property var replaceCalls: []
            property var parentOverrides: new Map()

            function isBinary(kind) {
                return kind === "add" || kind === "subtract" || kind === "multiply" || kind === "divide";
            }
            function childSlotsFor(kind) {
                if (isBinary(kind)) return ["left", "right"];
                if (kind === "runningSum" || kind === "rollingMean" || kind === "projectedFinalValue" || kind === "projectRateToFinal" || kind === "averageAcrossRuns") return ["input"];
                return [];
            }
            function describe(node) { return node ? node.kind : "…"; }
            function ancestorChain(node) {
                const chain = [];
                for (let current = node; current; current = parentOverrides.get(current)) chain.unshift(current);
                return chain;
            }
            function select(node) { selected = node; }
            function deleteNode(node) { deleteCalls.push(node); }
            function wrapSelected(kind) { wrapCalls.push(kind); }
            function replaceChild(parent, slot, kind) { replaceCalls.push({ parent: parent, slot: slot, kind: kind }); }
        }
    }
}
