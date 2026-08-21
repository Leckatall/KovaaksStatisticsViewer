import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase

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
            toExpressionMap: function () {
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
                return fakeEditor({
                    kind: "constant",
                    value: this.next
                });
            },
            updateComputedSeries: function () {
                this.updateCalls.push(Array.from(arguments));
            }
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
        compare(dialog.settingsVm.updateCalls[0][5].value, 1);
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
