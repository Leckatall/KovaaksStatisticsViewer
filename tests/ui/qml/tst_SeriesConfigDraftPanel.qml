import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase

    // TestCase.findChild doesn't reliably reach items nested under Repeater
    // delegates in this tree (same issue documented in tst_SettingsDialog.qml),
    // so lookups here walk the tree by hand.
    function findByObjectName(root, name) {
        if (!root)
            return null;
        if (root.objectName === name)
            return root;
        if (root.children) {
            for (const child of root.children) {
                const found = findByObjectName(child, name);
                if (found)
                    return found;
            }
        }
        return null;
    }
    function makeFakeSettingsVm(overrides) {
        const vm = Object.assign({
            allSeriesConfigs: [
                {
                    id: "1",
                    name: "Score",
                    color: "#009600",
                    width: 2,
                    enabled: true,
                    displayPosition: 0,
                    isPrimitive: true,
                    expression: {
                        kind: "primitive",
                        primitiveMetric: "score"
                    }
                }
            ],
            pendingChanges: false,
            commitDraftCalls: 0,
            discardDraftCalls: 0,
            seriesEnabledCalls: 0,
            createCalls: [],
            updateCalls: [],
            removeCalls: [],
            reorderCalls: [],
            beginSeriesDraft: function () {},
            commitSeriesDraft: function () {
                this.commitDraftCalls++;
                return {
                    succeeded: true
                };
            },
            discardSeriesDraft: function () {
                this.discardDraftCalls++;
            },
            setSeriesEnabled: function () {
                this.seriesEnabledCalls++;
            },
            createComputedSeries: function () {
                this.createCalls.push(Array.from(arguments));
            },
            updateComputedSeries: function () {
                this.updateCalls.push(Array.from(arguments));
            },
            removeComputedSeries: function () {
                this.removeCalls.push(Array.from(arguments));
            },
            reorderSeries: function () {
                this.reorderCalls.push(Array.from(arguments));
            },
            beginExpressionEdit: function () {
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
                        return {
                            kind: "primitive",
                            primitiveMetric: "score"
                        };
                    }
                };
            }
        }, overrides);
        return vm;
    }
    function rowsOutOfOrder() {
        return [
            {
                id: "2",
                name: "Second",
                color: "blue",
                width: 3,
                enabled: false,
                displayPosition: 1,
                isPrimitive: false,
                expression: {
                    kind: "constant",
                    value: 2
                }
            },
            {
                id: "1",
                name: "First",
                color: "red",
                width: 2,
                enabled: true,
                displayPosition: 0,
                isPrimitive: true,
                expression: {
                    kind: "primitive",
                    primitiveMetric: "score"
                }
            }
        ];
    }
    function test_acceptingColorDialogCallsUpdateComputedSeriesWithNewColorAndOtherFieldsUnchanged() {
        const vm = makeFakeSettingsVm();
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        mouseClick(findByObjectName(panel, "seriesColorSwatch_1"));
        panel.colorDialog.selectedColor = "#123456";
        panel.colorDialog.accepted();
        const call = panel.settingsVm.updateCalls[0];
        compare(call[0], "1");
        compare(call[1], "Score");
        compare(call[2].toString(), "#123456");
        compare(call[3], 2);
        compare(call[4], true);
        compare(call[5], vm.allSeriesConfigs[0].expression);
    }
    function test_addSeriesButtonCallsCreateComputedSeriesWithStubDefaults() {
        const vm = makeFakeSettingsVm();
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        findByObjectName(panel, "addSeriesButton").clicked();
        const call = panel.settingsVm.createCalls[0];
        compare(call[0], "New Series");
        compare(call[1], "#4CAF50");
        compare(call[2], 2);
        compare(call[3], true);
        compare(call[4], {
            kind: "primitive",
            primitiveMetric: "score"
        });
    }
    function test_clickingDiscardCallsDiscardSeriesDraft() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                pendingChanges: true
            })
        });
        verify(waitForRendering(panel));

        mouseClick(findByObjectName(panel, "discardGraphLineChangesButton"));

        compare(panel.settingsVm.discardDraftCalls, 1);
    }
    function test_clickingSaveCallsCommitSeriesDraft() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                pendingChanges: true
            })
        });
        verify(waitForRendering(panel));

        mouseClick(findByObjectName(panel, "saveGraphLinesButton"));

        compare(panel.settingsVm.commitDraftCalls, 1);
    }
    function test_colorSwatchClickOpensColorDialogSeededWithRowColor() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm()
        });
        verify(waitForRendering(panel));
        mouseClick(findByObjectName(panel, "seriesColorSwatch_1"));
        compare(panel.colorDialog.selectedColor.toString(), "#009600");
    }
    function test_deleteButtonCallsRemoveComputedSeriesWithRowId() {
        const vm = makeFakeSettingsVm();
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        findByObjectName(panel, "deleteSeriesButton_1").clicked();
        compare(panel.settingsVm.removeCalls[0], ["1"]);
    }
    function test_deleteButtonWorksIdenticallyOnAPrimitiveRow() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        findByObjectName(panel, "deleteSeriesButton_1").clicked();
        compare(panel.settingsVm.removeCalls[0], ["1"]);
    }
    function test_draggingARowDoesNotCallReorderSeriesUntilReleased() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = findByObjectName(panel, "seriesDragHandle_1");
        const start = handle.mapToItem(panel, handle.width / 2, handle.height / 2);
        const rowHeight = handle.parent.height;
        mousePress(panel, start.x, start.y);
        wait(20);
        for (let step = 1; step <= 8; step++) {
            mouseMove(panel, start.x, start.y + rowHeight * step / 8, -1, Qt.LeftButton);
            wait(10);
        }
        compare(panel.settingsVm.reorderCalls.length, 0);
        mouseRelease(panel, start.x, start.y + rowHeight);
        wait(20);
    }
    function test_draggingInMultipleIncrementsStillReordersToTheCorrectFinalPositionOnRelease() {
        const rows = rowsOutOfOrder().concat([
            {
                id: "3",
                name: "Third",
                color: "green",
                width: 2,
                enabled: true,
                displayPosition: 2,
                isPrimitive: false,
                expression: {
                    kind: "constant",
                    value: 3
                }
            },
            {
                id: "4",
                name: "Fourth",
                color: "yellow",
                width: 2,
                enabled: true,
                displayPosition: 3,
                isPrimitive: false,
                expression: {
                    kind: "constant",
                    value: 4
                }
            }
        ]);
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rows
        });
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = findByObjectName(panel, "seriesDragHandle_1");
        const start = handle.mapToItem(panel, handle.width / 2, handle.height / 2);
        const rowHeight = handle.parent.height;
        mousePress(panel, start.x, start.y);
        wait(20);
        for (let step = 1; step <= 8; step++) {
            mouseMove(panel, start.x, start.y + rowHeight * 0.8 * step / 8, -1, Qt.LeftButton);
            wait(10);
        }
        for (let step = 1; step <= 8; step++) {
            mouseMove(panel, start.x, start.y + rowHeight * (0.8 + 0.4 * step / 8), -1, Qt.LeftButton);
            wait(10);
        }
        mouseRelease(panel, start.x, start.y + rowHeight * 1.2);
        wait(20);
        compare(panel.settingsVm.reorderCalls.length, 1);
        compare(panel.settingsVm.reorderCalls[0], ["1", 1]);
    }
    function test_editExpressionButtonOpensExpressionEditorDialogWithThisRowsSeriesId() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        mouseClick(findByObjectName(panel, "editExpressionButton_2"));
        tryCompare(panel.expressionDialog, "visible", true);
        compare(panel.expressionDialog.seriesId, "2");
        compare(panel.expressionDialog.seriesName, "Second");
        compare(panel.expressionDialog.seriesColor.toString(), "#0000ff");
        compare(panel.expressionDialog.seriesWidth, 3);
        compare(panel.expressionDialog.seriesEnabled, false);
    }
    function test_editExpressionButtonWorksIdenticallyOnAPrimitiveRow() {
        const vm = makeFakeSettingsVm();
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        mouseClick(findByObjectName(panel, "editExpressionButton_1"));
        tryCompare(panel.expressionDialog, "visible", true);
        compare(panel.expressionDialog.seriesId, "1");
    }
    function test_editingNameFieldAndCommittingCallsUpdateComputedSeriesWithNewNameAndOtherFieldsUnchanged() {
        const vm = makeFakeSettingsVm();
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const field = findByObjectName(panel, "seriesNameField_1");
        field.text = "New score";
        field.editingFinished();
        const call = panel.settingsVm.updateCalls[0];
        compare(call[0], "1");
        compare(call[1], "New score");
        compare(call[2].toString(), "#009600");
        compare(call[3], 2);
        compare(call[4], true);
        compare(call[5], vm.allSeriesConfigs[0].expression);
    }
    function test_everyRowShowsAColorSwatchNameFieldEditExpressionButtonAndDeleteButtonIncludingPrimitiveRows() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        for (const id of ["1", "2"]) {
            verify(findByObjectName(panel, "seriesColorSwatch_" + id) !== null);
            verify(findByObjectName(panel, "seriesNameField_" + id) !== null);
            verify(findByObjectName(panel, "editExpressionButton_" + id) !== null);
            verify(findByObjectName(panel, "deleteSeriesButton_" + id) !== null);
        }
    }
    function test_releasingADraggedRowCallsReorderSeriesOnceWithTheFinalPosition() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = findByObjectName(panel, "seriesDragHandle_1");
        const start = handle.mapToItem(panel, handle.width / 2, handle.height / 2);
        const rowHeight = handle.parent.height;
        mousePress(panel, start.x, start.y);
        wait(20);
        for (let step = 1; step <= 8; step++) {
            mouseMove(panel, start.x, start.y + rowHeight * step / 8, -1, Qt.LeftButton);
            wait(10);
        }
        mouseRelease(panel, start.x, start.y + rowHeight);
        wait(20);
        compare(panel.settingsVm.reorderCalls.length, 1);
        compare(panel.settingsVm.reorderCalls[0], ["1", 1]);
    }
    function test_rowsRenderedOrderedByDisplayPosition() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        compare(panel.displayRows[0].id, "1");
        compare(panel.displayRows[1].id, "2");
    }
    function test_saveAndDiscardButtonsDisabledWithoutPendingChanges() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                pendingChanges: false
            })
        });
        verify(waitForRendering(panel));

        const saveButton = findByObjectName(panel, "saveGraphLinesButton");
        const discardButton = findByObjectName(panel, "discardGraphLineChangesButton");
        verify(saveButton !== null);
        verify(discardButton !== null);
        compare(saveButton.enabled, false);
        compare(discardButton.enabled, false);
    }
    function test_saveAndDiscardButtonsEnabledWithPendingChanges() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm({
                pendingChanges: true
            })
        });
        verify(waitForRendering(panel));

        const saveButton = findByObjectName(panel, "saveGraphLinesButton");
        const discardButton = findByObjectName(panel, "discardGraphLineChangesButton");
        compare(saveButton.enabled, true);
        compare(discardButton.enabled, true);
    }
    function test_togglingSeriesEnabledSwitchWritesThroughSettingsVm() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            settingsVm: makeFakeSettingsVm()
        });

        const scoreSwitch = findByObjectName(panel.contentItem, "seriesEnabledSwitch_1");
        verify(scoreSwitch !== null);
        mouseClick(scoreSwitch);

        tryCompare(scoreSwitch, "checked", false);
        compare(panel.settingsVm.seriesEnabledCalls, 1);
        // compare(panel.settingsVm.seriesEnabledCalls[0].id, "2")
        // compare(panel.settingsVm.seriesEnabledCalls[0].enabled, true)
    }

    height: 400
    name: "SeriesConfigDraftPanelTest"
    visible: true
    when: windowShown
    width: 400

    Component {
        id: panelComponent

        SeriesConfigDraftPanel {
        }
    }
}
