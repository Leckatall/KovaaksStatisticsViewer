import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "SeriesConfigDraftPanelTest"
    when: windowShown
    width: 400
    height: 400
    visible: true

    Component {
        id: panelComponent
        SeriesConfigDraftPanel {}
    }

    function makeFakeSettingsVm(overrides) {
        return Object.assign({
            allSeriesConfigs: [
                {id: "1", name: "Score", color: "#009600", enabled: true, displayPosition: 0}
            ],
            pendingChanges: false,
            beginDraftCalls: 0,
            commitDraftCalls: 0,
            discardDraftCalls: 0,
            seriesEnabledCalls: 0,
            beginSeriesDraft: function () { this.beginDraftCalls++ },
            commitSeriesDraft: function () { this.commitDraftCalls++; return {succeeded: true} },
            discardSeriesDraft: function () { this.discardDraftCalls++ },
            setSeriesEnabled: function () { this.seriesEnabledCalls++},
        }, overrides)
    }

    // TestCase.findChild doesn't reliably reach items nested under Repeater
    // delegates in this tree (same issue documented in tst_SettingsDialog.qml),
    // so lookups here walk the tree by hand.
    function findByObjectName(root, name) {
        if (!root) return null
        if (root.objectName === name) return root
        if (root.children) {
            for (const child of root.children) {
                const found = findByObjectName(child, name)
                if (found) return found
            }
        }
        return null
    }

    function test_beginSeriesDraftCalledOnceOnInstantiation() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm()})
        verify(panel !== null)
        compare(panel.settingsVm.beginDraftCalls, 1)
    }

    function test_saveAndDiscardButtonsDisabledWithoutPendingChanges() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm({pendingChanges: false})})
        verify(waitForRendering(panel))

        const saveButton = findByObjectName(panel, "saveGraphLinesButton")
        const discardButton = findByObjectName(panel, "discardGraphLineChangesButton")
        verify(saveButton !== null)
        verify(discardButton !== null)
        compare(saveButton.enabled, false)
        compare(discardButton.enabled, false)
    }

    function test_saveAndDiscardButtonsEnabledWithPendingChanges() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm({pendingChanges: true})})
        verify(waitForRendering(panel))

        const saveButton = findByObjectName(panel, "saveGraphLinesButton")
        const discardButton = findByObjectName(panel, "discardGraphLineChangesButton")
        compare(saveButton.enabled, true)
        compare(discardButton.enabled, true)
    }

    function test_clickingSaveCallsCommitSeriesDraft() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm({pendingChanges: true})})
        verify(waitForRendering(panel))

        mouseClick(findByObjectName(panel, "saveGraphLinesButton"))

        compare(panel.settingsVm.commitDraftCalls, 1)
    }

    function test_clickingDiscardCallsDiscardSeriesDraft() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm({pendingChanges: true})})
        verify(waitForRendering(panel))

        mouseClick(findByObjectName(panel, "discardGraphLineChangesButton"))

        compare(panel.settingsVm.discardDraftCalls, 1)
    }

    function test_togglingSeriesEnabledSwitchWritesThroughSettingsVm() {
        const panel = createTemporaryObject(panelComponent, testCase, {settingsVm: makeFakeSettingsVm()})

        const scoreSwitch = findByObjectName(panel.contentItem, "seriesEnabledSwitch_1")
        verify(scoreSwitch !== null)
        mouseClick(scoreSwitch)

        tryCompare(scoreSwitch, "checked", false)
        compare(panel.settingsVm.seriesEnabledCalls, 1)
        // compare(panel.settingsVm.seriesEnabledCalls[0].id, "2")
        // compare(panel.settingsVm.seriesEnabledCalls[0].enabled, true)
    }
}
