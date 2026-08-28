import QtQuick
import QtQuick.Controls
import QtTest
import KovaaksStatsViewer
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

    function fakeEditor(expression) {
        return {
            root: {},
            selectedNodeId: "",
            treeRevision: 0,
            nodeKinds: ["primitive"],
            primitiveMetrics: ["score"],
            isBinary: function () {
                return false;
            },
            childSlotsFor: function () {
                return [];
            },
            describe: function () {
                return "";
            },
            ancestorChain: function () {
                return [];
            },
            select: function () {},
            replaceChild: function () {},
            deleteNode: function () {},
            wrapSelected: function () {},
            changeBinaryOperator: function () {},
            updateField: function () {},
            changeSelectionKind: function () {},
            toDslText: function () {
                return expression;
            }
        };
    }
    function fakeVm() {
        return {
            beginExpressionEditCalls: [],
            updateCalls: [],
            next: 0,
            beginExpressionEdit: function (id) {
                this.beginExpressionEditCalls.push(id);
                this.next++;
                return fakeEditor(String(this.next));
            },
            updateComputedSeries: function () {
                this.updateCalls.push(Array.from(arguments));
            }
        };
    }
    function fakeDeepEditor() {
        const leaf = Qt.createQmlObject('import KovaaksStatsViewer; EditablePrimitiveNode { metric: "score" }', testCase, "deepLeaf");
        const chain = [leaf];
        let child = leaf;
        for (const window of [8, 7, 1, 6, 2, 4, 3, 5]) {
            child = Qt.createQmlObject('import KovaaksStatsViewer; EditableRollingMeanNode {}', testCase, "deepRollingMean");
            child.window = window;
            child.input = chain[chain.length - 1];
            chain.push(child);
        }
        const root = chain[chain.length - 1];
        return {
            root: root,
            selected: leaf,
            nodeKinds: ["primitive", "rollingMean"],
            primitiveMetrics: ["score"],
            isBinary: function () { return false; },
            childSlotsFor: function (kind) { return kind === "rollingMean" ? ["input"] : []; },
            describe: function (node) { return node.kind === "primitive" ? node.metric : node.kind; },
            ancestorChain: function () { return chain.slice().reverse(); },
            select: function (node) { this.selected = node; },
            replaceChild: function () {},
            deleteNode: function () {},
            wrapSelected: function () {},
            changeBinaryOperator: function () {},
            updateField: function () {},
            changeSelectionKind: function () {},
            toDslText: function () { return "SCORE"; }
        };
    }
    function makeDialog(vm) {
        return createTemporaryObject(dialogComponent, testCase, {
            settingsVm: vm,
            seriesId: "7",
            seriesName: "Aim",
            seriesColor: "red",
            seriesWidth: 3,
            seriesEnabled: false
        });
    }
    function test_clickingCancelDoesNotCallUpdateComputedSeries() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        wait(20);
        dialog.reject();
        compare(dialog.settingsVm.updateCalls.length, 0);
    }
    function test_clickingSaveCallsUpdateComputedSeriesWithEditorExpression() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        wait(20);
        dialog.accept();
        compare(dialog.settingsVm.updateCalls.length, 1);
        compare(dialog.settingsVm.updateCalls[0][0], "7");
        compare(dialog.settingsVm.updateCalls[0][5], "1");
    }
    function test_closingWithoutAcceptingDoesNotCallUpdateComputedSeries() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.open();
        tryCompare(dialog, "visible", true);
        dialog.close();
        tryCompare(dialog, "visible", false);
        compare(vm.updateCalls.length, 0);
    }
    function test_dialogCanBeCreatedBeforeItsFirstOpenWithoutQmlErrors() {
        const dialog = makeDialog(fakeVm());
        verify(dialog !== null);
        verify(findChild(dialog, "expressionEditorTreeEditor") === null);
    }
    function test_dialogClampsAndScrollsARealDeepTreeOpenedThroughTheRealDialog() {
        const vm = fakeVm();
        vm.beginExpressionEdit = function (id) {
            this.beginExpressionEditCalls.push(id);
            return fakeDeepEditor();
        };
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        dialog.open();
        tryCompare(dialog, "visible", true);
        wait(20);

        compare(dialog.height, Math.max(0, Overlay.overlay.height - dialog.overlayMargin));
        const treeEditor = findChild(dialog, "expressionEditorTreeEditor");
        verify(treeEditor !== null);
        const scrollView = findByObjectName(treeEditor, "expressionTreeScrollView");
        verify(scrollView !== null);
        verify(findByObjectName(treeEditor, "pathCard_0") !== null);
        verify(scrollView.contentHeight > scrollView.height);
        const contentYBeforeScroll = scrollView.contentItem.contentY;
        scrollView.contentItem.contentY = scrollView.contentHeight - scrollView.height;
        wait(20);
        verify(scrollView.contentItem.contentY > contentYBeforeScroll);
    }
    function test_dialogWidthAndHeightAreClampedToTheOverlaySize() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        dialog.open();
        tryCompare(dialog, "visible", true);
        wait(20);
        verify(dialog.width <= testCase.width);
        verify(dialog.height <= testCase.height);
    }
    function test_openingDialogCallsBeginExpressionEditWithTheGivenSeriesId() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        wait(20);
        compare(dialog.settingsVm.beginExpressionEditCalls, ["7"]);
    }
    function test_reopeningDialogRequestsAFreshEditorModelEachTime() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.beginEditing();
        wait(20);
        dialog.beginEditing();
        wait(20);
        compare(dialog.settingsVm.beginExpressionEditCalls.length, 2);
    }
    function test_treeEditorInsideDialogReceivesTheEditorModelReturnedByBeginExpressionEdit() {
        const vm = fakeVm();
        const dialog = makeDialog(vm);
        dialog.open();
        tryCompare(dialog, "visible", true);
        wait(20);
        const treeEditor = findChild(dialog, "expressionEditorTreeEditor");
        verify(treeEditor !== null);
        compare(treeEditor.model, dialog.editorModel);
    }

    height: 500
    name: "ExpressionEditorDialogTest"
    visible: true
    when: windowShown
    width: 600

    Component {
        id: dialogComponent

        ExpressionEditorDialog {
        }
    }
}
