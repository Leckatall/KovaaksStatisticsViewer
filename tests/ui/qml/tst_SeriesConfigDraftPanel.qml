import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/series"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase

    Component {
        id: externalFixture
        ScrollView {
            id: page
            property var vm
            property alias panel: panel
            width: 400
            height: 400
            contentWidth: availableWidth
            SeriesConfigDraftPanel {
                id: panel
                width: page.availableWidth
                settingsVm: page.vm
                scrollView: page
            }
        }
    }
    function test_thePanelHasNoScrollViewOfItsOwnAndGrowsWithItsRows() {
        const page = createTemporaryObject(externalFixture, testCase, {
            vm: makeFakeSettingsVm({allSeriesConfigs: manyRows()})
        })
        verify(waitForRendering(page))
        compare(ItemLookup.findByObjectName(page.panel, "seriesListScrollView"), null)
        const row = ItemLookup.findByObjectName(page.panel, "seriesNameField_19")
        const button = ItemLookup.findByObjectName(page.panel, "addSeriesButton")
        verify(button.mapToItem(page.panel, 0, 0).y > row.mapToItem(page.panel, 0, 0).y)
        verify(page.contentItem.contentHeight > page.height)
    }
    function test_aHeaderRowNamesEachColumnAboveItsControl_data() {
        return [{tag: "compact", width: 400}, {tag: "wide", width: 700}]
    }
    function test_aHeaderRowNamesEachColumnAboveItsControl(data) {
        const page = createTemporaryObject(externalFixture, testCase, {
            vm: makeFakeSettingsVm({allSeriesConfigs: rowsOutOfOrder()}), width: data.width
        })
        verify(waitForRendering(page))
        const columns = [["name", "seriesNameField_1"],
                         ["expression", "editExpressionButton_1"],
                         ["axis", "axisCombo_1"],
                         ["more", "seriesOptionsButton_1"],
                         ["visible", "seriesEnabledSwitch_1"]]
        for (const [name, controlName] of columns) {
            const header = ItemLookup.findByObjectName(page.panel, "seriesColumnHeader_" + name)
            const control = ItemLookup.findByObjectName(page.panel, controlName)
            verify(header !== null)
            verify(header.Accessible.ignored)
            const hp = header.mapToItem(page.panel, header.width / 2, 0)
            const cp = control.mapToItem(page.panel, 0, 0)
            verify(hp.x >= cp.x && hp.x <= cp.x + control.width, name + ": " + hp.x + " vs " + cp.x + ".." + (cp.x + control.width))
            verify(hp.y < cp.y)
        }
    }

    function makePanel(properties) {
        const page = createTemporaryObject(externalFixture, testCase, {
            vm: properties.settingsVm, height: properties.height || 400
        })
        return page.panel
    }
    function makeFakeSettingsVm(overrides) {
        return TestDoubles.makeFakeSettingsVm(Object.assign({
            allSeriesConfigs: [
                {
                    id: "1",
                    name: "Score",
                    color: "#009600",
                    width: 2,
                    enabled: true,
                    displayPosition: 0,
                    hiddenLeadingPoints: 0,
                    yAxisId: "",
                    expression: "SCORE"
                }
            ],
            createCalls: [],
            updateCalls: [],
            removeCalls: [],
            reorderCalls: [],
            updateSeriesAxisCalls: [],
            restoreDefaultGraphLinesCalls: 0,
            restoreDefaultGraphLines: function () {
                this.restoreDefaultGraphLinesCalls++;
                return {
                    succeeded: true
                };
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
            updateSeriesAxis: function () {
                this.updateSeriesAxisCalls.push(Array.from(arguments));
                return {
                    succeeded: true
                };
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
                    toDslText: function () { return "SCORE"; }
                };
            }
        }, overrides));
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
                hiddenLeadingPoints: 0,
                expression: "2"
            },
            {
                id: "1",
                name: "First",
                color: "red",
                width: 2,
                enabled: true,
                displayPosition: 0,
                hiddenLeadingPoints: 2,
                expression: "SCORE"
            }
        ];
    }
    function manyRows() {
        const rows = [];
        for (let i = 0; i < 20; i++) {
            rows.push({
                id: String(i),
                name: "Series " + i,
                color: "#009600",
                width: 2,
                enabled: true,
                displayPosition: i,
                expression: "SCORE"
            });
        }
        return rows;
    }
    function test_acceptingColorDialogCallsUpdateComputedSeriesWithNewColorAndOtherFieldsUnchanged() {
        const vm = makeFakeSettingsVm();
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        mouseClick(ItemLookup.findByObjectName(panel, "seriesColorSwatch_1"));
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
    function test_addSeriesButtonCreatesABlankSeriesWithoutAuthoringAnExpression() {
        const vm = makeFakeSettingsVm();
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        ItemLookup.findByObjectName(panel, "addSeriesButton").clicked();
        const call = panel.settingsVm.createCalls[0];
        compare(call[4], "");
    }
    function test_restoreDefaultLinesButtonCallsRestoreDefaultGraphLines() {
        const vm = makeFakeSettingsVm();
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const button = ItemLookup.findByObjectName(panel, "restoreDefaultGraphLinesButton");
        verify(button);
        compare(panel.settingsVm.restoreDefaultGraphLinesCalls, 0);
        button.clicked();
        compare(panel.settingsVm.restoreDefaultGraphLinesCalls, 1);
    }
    function test_axisComboDefaultsToIndependentWhenRowHasNoYAxisId() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm()
        });
        verify(waitForRendering(panel));
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");
        verify(combo !== null);
        compare(combo.currentText, "Independent");
    }
    function test_selectingAnExistingAxisCallsUpdateSeriesAxis() {
        const vm = makeFakeSettingsVm({
            allAxes: [{
                id: "2",
                name: "Score Family"
            }]
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");
        panel.handleAxisSelection(panel.displayRows[0], "2");
        compare(panel.settingsVm.updateSeriesAxisCalls[0], ["1", "2"]);
    }
    function test_selectingIndependentClearsAnAssignedAxis() {
        const vm = makeFakeSettingsVm({
            allAxes: [{
                id: "2",
                name: "Score Family"
            }],
            allSeriesConfigs: [{
                id: "1",
                name: "Score",
                color: "#009600",
                width: 2,
                enabled: true,
                displayPosition: 0,
                yAxisId: "2",
                expression: "SCORE"
            }]
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");
        compare(combo.currentText, "Score Family");
        panel.handleAxisSelection(panel.displayRows[0], "");
        compare(panel.settingsVm.updateSeriesAxisCalls[0], ["1", ""]);
    }
    function test_selectingCreateNewAxisOpensDialogAndCommitsOnAccept() {
        const vm = makeFakeSettingsVm({
            allAxes: []
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");
        panel.handleAxisSelection(panel.displayRows[0], "__new__");
        const dialog = panel.newAxisDialog;
        verify(dialog !== null);
        panel.newAxisNameField.text = "Custom";
        const create = ItemLookup.findByObjectName(dialog.footer, "newAxisCreateButton");
        create.clicked();
        compare(dialog.settingsVm.createAxisCalls, ["Custom"]);
        compare(dialog.settingsVm.updateSeriesAxisCalls[0], ["1", "9"]);
    }
    // Choosing an axis republishes the lines, which rebuilds the row whose dropdown is still handling
    // the activation.
    function test_choosingAnAxisInTheDropdownRebuildsItsRowCleanly() {
        let panel = null;
        const vm = makeFakeSettingsVm({
            allAxes: [{id: "2", name: "Score Family"}],
            updateSeriesAxis: function (id, axisId) {
                this.updateSeriesAxisCalls.push([id, axisId]);
                panel.sourceRows = panel.sourceRows.map(row => Object.assign({}, row, {
                    yAxisId: row.id === id ? axisId : row.yAxisId
                }));
                return {succeeded: true};
            }
        });
        panel = makePanel({settingsVm: vm});
        verify(waitForRendering(panel));
        failOnWarning(/.*/);
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");

        combo.currentIndex = 1;
        combo.activated(1);

        compare(panel.settingsVm.updateSeriesAxisCalls, [["1", "2"]]);
        tryVerify(() => ItemLookup.findByObjectName(panel, "axisCombo_1") !== null);
        const rebuilt = ItemLookup.findByObjectName(panel, "axisCombo_1");
        verify(rebuilt !== combo, "the activation rebuilt the row");
        compare(rebuilt.currentText, "Score Family");
    }
    function test_cancellingNewAxisRestoresTheLinesAxisDropdown() {
        const vm = makeFakeSettingsVm({
            allAxes: [{id: "2", name: "Score Family"}],
            allSeriesConfigs: [{
                id: "1", name: "Score", color: "#009600", width: 2, enabled: true,
                displayPosition: 0, hiddenLeadingPoints: 0, yAxisId: "2", expression: "SCORE"
            }]
        });
        const panel = makePanel({settingsVm: vm});
        verify(waitForRendering(panel));
        const combo = ItemLookup.findByObjectName(panel, "axisCombo_1");
        combo.currentIndex = combo.count - 1;
        combo.activated(combo.currentIndex);
        const cancel = ItemLookup.findByObjectName(panel.newAxisDialog.footer, "newAxisCancelButton");
        cancel.clicked();
        compare(combo.currentText, "Score Family");
        compare(panel.newAxisDialog.settingsVm.createAxisCalls.length, 0);
    }
    function test_colorSwatchClickOpensColorDialogSeededWithRowColor() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm()
        });
        verify(waitForRendering(panel));
        mouseClick(ItemLookup.findByObjectName(panel, "seriesColorSwatch_1"));
        compare(panel.colorDialog.selectedColor.toString(), "#009600");
    }
    function openOptionsPopup(panel, id) {
        mouseClick(ItemLookup.findByObjectName(panel, "seriesOptionsButton_" + id));
        tryVerify(function () {
            const item = ItemLookup.findByObjectName(Overlay.overlay, "deleteSeriesButton_" + id);
            return item !== null && item.visible;
        });
        return Overlay.overlay;
    }
    function test_deleteButtonCallsRemoveComputedSeriesWithRowId() {
        const vm = makeFakeSettingsVm();
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const overlay = openOptionsPopup(panel, "1");
        mouseClick(ItemLookup.findByObjectName(overlay, "deleteSeriesButton_1"));
        compare(panel.settingsVm.removeCalls[0], ["1"]);
    }
    function test_changingTheHiddenLeadingPointsSpinBoxCallsSetSeriesHiddenLeadingPointsWithIdAndValue() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const overlay = openOptionsPopup(panel, "1");
        const spinBox = ItemLookup.findByObjectName(overlay, "hiddenLeadingPointsSpinBox_1");
        verify(spinBox !== null);
        compare(spinBox.value, 2);
        compare(spinBox.to, vm.maximumHiddenLeadingPoints);
        mouseClick(spinBox.up.indicator);
        compare(panel.settingsVm.setSeriesHiddenLeadingPointsCalls, [["1", 3]]);
    }
    function test_theOptionsPopupOpensUnderItsButtonWithAHintThatReflectsTheCount() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        const button = ItemLookup.findByObjectName(panel, "seriesOptionsButton_1");
        const overlay = openOptionsPopup(panel, "1");
        const corner = button.mapToItem(panel, button.width, button.height);
        compare(panel.optionsAnchorRight, corner.x);
        compare(panel.optionsAnchorBottom, corner.y);
        verify(panel.optionsPopup.y > corner.y);
        compare(ItemLookup.findByObjectName(overlay, "hiddenLeadingPointsHint").text,
                "The line starts after its first 2 points.");
    }
    function test_deletingASeriesClosesTheOptionsPopup() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        const overlay = openOptionsPopup(panel, "2");
        mouseClick(ItemLookup.findByObjectName(overlay, "deleteSeriesButton_2"));
        compare(panel.settingsVm.removeCalls[0], ["2"]);
        tryVerify(function () { return !panel.optionsPopup.visible; });
    }
    function test_theOptionsPopupClosesWhenItsSeriesDisappears() {
        const vm = createTemporaryObject(liveSettingsVmComponent, testCase, {
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        openOptionsPopup(panel, "1");
        vm.allSeriesConfigs = vm.allSeriesConfigs.filter(function (row) { return row.id !== "1"; });
        tryVerify(function () { return !panel.optionsPopup.visible; });
        compare(panel.optionsTargetId, "");
    }
    function test_theOptionsPopupStaysOpenAcrossEditsThatRepublishTheRows() {
        const vm = createTemporaryObject(liveSettingsVmComponent, testCase, {
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const overlay = openOptionsPopup(panel, "1");
        mouseClick(ItemLookup.findByObjectName(overlay, "hiddenLeadingPointsSpinBox_1").up.indicator);
        compare(vm.setSeriesHiddenLeadingPointsCalls, [["1", 3]]);
        wait(50);

        const spinBox = ItemLookup.findByObjectName(overlay, "hiddenLeadingPointsSpinBox_1");
        verify(spinBox !== null && spinBox.visible, "the options popup closed after one step");
        compare(spinBox.value, 3);
        mouseClick(spinBox.up.indicator);
        compare(vm.setSeriesHiddenLeadingPointsCalls, [["1", 3], ["1", 4]]);
        wait(50);
        compare(ItemLookup.findByObjectName(overlay, "hiddenLeadingPointsSpinBox_1").value, 4);
        verify(ItemLookup.findByObjectName(panel, "seriesOptionsIndicator_1").visible);
    }
    function test_draggingARowDoesNotCallReorderSeriesUntilReleased() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_1");
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
                expression: "3"
            },
            {
                id: "4",
                name: "Fourth",
                color: "yellow",
                width: 2,
                enabled: true,
                displayPosition: 3,
                expression: "4"
            }
        ]);
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rows
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_1");
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
    function test_draggingNearThePageBottomEdgeAutoScrollsThePage() {
        const panel = makePanel({settingsVm: makeFakeSettingsVm({allSeriesConfigs: manyRows()}), height: 250})
        const page = panel.scrollView
        verify(waitForRendering(page))
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_0")
        const start = handle.mapToItem(page, handle.width / 2, handle.height / 2)
        mousePress(page, start.x, start.y)
        wait(20)
        for (let step = 1; step <= 8; ++step) {
            mouseMove(page, start.x, start.y + (page.height - 5 - start.y) * step / 8, -1, Qt.LeftButton)
            wait(10)
        }
        wait(300)
        verify(page.contentItem.contentY > 0)
        const first = panel.dragPreviewIndex
        wait(300)
        verify(panel.dragPreviewIndex > first)
        mouseRelease(page, start.x, page.height - 5)
    }
    function test_droppingAfterThePageAutoScrolledReordersToTheLastPosition() {
        const vm = makeFakeSettingsVm({allSeriesConfigs: manyRows()})
        const panel = makePanel({settingsVm: vm, height: 250})
        const page = panel.scrollView
        verify(waitForRendering(page))
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_0")
        const start = handle.mapToItem(page, handle.width / 2, handle.height / 2)
        mousePress(page, start.x, start.y)
        wait(20)
        for (let step = 1; step <= 8; ++step) {
            mouseMove(page, start.x, start.y + (page.height - 5 - start.y) * step / 8, -1, Qt.LeftButton)
            wait(10)
        }
        tryVerify(() => page.contentItem.contentY >= page.contentItem.contentHeight - page.contentItem.height, 5000)
        compare(panel.draggedSeriesId, "0")
        const releasePoint = page.mapToItem(panel, start.x, page.height - 5)
        mouseRelease(panel, releasePoint.x, releasePoint.y)
        wait(20)
        compare(panel.settingsVm.reorderCalls.length, 1)
        compare(panel.settingsVm.reorderCalls[0], ["0", 19])
    }
    function test_editExpressionButtonOpensExpressionEditorDialogWithThisRowsSeriesId() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        mouseClick(ItemLookup.findByObjectName(panel, "editExpressionButton_2"));
        tryCompare(panel.expressionDialog, "visible", true);
        compare(panel.expressionDialog.seriesId, "2");
        compare(panel.expressionDialog.seriesName, "Second");
        compare(panel.expressionDialog.seriesColor.toString(), "#0000ff");
        compare(panel.expressionDialog.seriesWidth, 3);
        compare(panel.expressionDialog.seriesEnabled, false);
    }
    function test_editingNameFieldAndCommittingCallsUpdateComputedSeriesWithNewNameAndOtherFieldsUnchanged() {
        const vm = makeFakeSettingsVm();
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const field = ItemLookup.findByObjectName(panel, "seriesNameField_1");
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
    function test_everyRowShowsAColorSwatchNameFieldEditExpressionButtonAndOptionsButtonIncludingPrimitiveRows() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        for (const id of ["1", "2"]) {
            verify(ItemLookup.findByObjectName(panel, "seriesColorSwatch_" + id) !== null);
            verify(ItemLookup.findByObjectName(panel, "seriesNameField_" + id) !== null);
            verify(ItemLookup.findByObjectName(panel, "editExpressionButton_" + id) !== null);
            verify(ItemLookup.findByObjectName(panel, "seriesOptionsButton_" + id) !== null);
        }
    }
    function test_reentrantModelResetDuringDragReleaseLeavesCleanDragState() {
        const vm = createTemporaryObject(reentrantSettingsVmComponent, testCase, {
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_1");
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
        compare(panel.draggedSeriesId, "");
        compare(panel.dragOriginIndex, -1);
        compare(panel.dragPreviewIndex, -1);
        compare(panel.dragTranslationY, 0);
    }
    function test_releasingADraggedRowCallsReorderSeriesOnceWithTheFinalPosition() {
        const vm = makeFakeSettingsVm({
            allSeriesConfigs: rowsOutOfOrder()
        });
        const panel = makePanel({
            settingsVm: vm
        });
        verify(waitForRendering(panel));
        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_1");
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
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        compare(panel.displayRows[0].id, "1");
        compare(panel.displayRows[1].id, "2");
    }
    function test_dragCursorOverlayIsEnabledOnlyWhileARowIsDragged() {
        const panel = makePanel({
            settingsVm: makeFakeSettingsVm({
                allSeriesConfigs: rowsOutOfOrder()
            })
        });
        verify(waitForRendering(panel));
        const overlay = ItemLookup.findByObjectName(panel, "seriesDragCursorOverlay");
        verify(overlay !== null);
        compare(overlay.enabled, false);

        const handle = ItemLookup.findByObjectName(panel, "seriesDragHandle_1");
        const start = handle.mapToItem(panel, handle.width / 2, handle.height / 2);
        mousePress(panel, start.x, start.y);
        mouseMove(panel, start.x, start.y + 20, -1, Qt.LeftButton);
        tryCompare(overlay, "enabled", true);
        mouseRelease(panel, start.x, start.y + 20);
        tryCompare(overlay, "enabled", false);
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

    Component {
        id: liveSettingsVmComponent

        QtObject {
            property var allSeriesConfigs: []
            property var allAxes: []
            property var issues: []
            property bool pendingChanges: false
            property int maximumHiddenLeadingPoints: 10
            property var setSeriesHiddenLeadingPointsCalls: []
            function setSeriesHiddenLeadingPoints(id, count) {
                setSeriesHiddenLeadingPointsCalls = setSeriesHiddenLeadingPointsCalls.concat([[id, count]])
                allSeriesConfigs = allSeriesConfigs.map(function(row) {
                    return row.id === id ? Object.assign({}, row, { hiddenLeadingPoints: count }) : Object.assign({}, row)
                })
                return { succeeded: true }
            }
        }
    }

    // Unlike makeFakeSettingsVm()'s plain JS object, allSeriesConfigs here is a real
    // QML property var: reassigning it fires the engine's automatic change notification,
    // matching the real SettingsViewModel's Q_PROPERTY(... NOTIFY seriesConfigurationChanged).
    Component {
        id: reentrantSettingsVmComponent

        QtObject {
            property var allSeriesConfigs: []
            property var allAxes: []
            property bool pendingChanges: false
            function reorderSeries(id, position) {
                const configs = allSeriesConfigs.slice().sort(function(a, b) { return a.displayPosition - b.displayPosition })
                const index = configs.findIndex(function(row) { return row.id === id })
                if (index === -1 || position >= configs.length || index === position) return
                const [row] = configs.splice(index, 1)
                configs.splice(position, 0, row)
                configs.forEach(function(row, i) { row.displayPosition = i })
                allSeriesConfigs = configs
            }
        }
    }
}
