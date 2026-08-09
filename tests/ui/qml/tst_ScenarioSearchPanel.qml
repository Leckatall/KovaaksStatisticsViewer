import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "ScenarioSearchPanelTest"
    when: windowShown
    width: 500
    height: 500
    visible: true

    Component {
        id: panelComponent
        ScenarioSearchPanel {}
    }

    SignalSpy {
        id: activatedSpy
        signalName: "scenarioActivated"
    }

    SignalSpy {
        id: searchSpy
        signalName: "searchEdited"
    }

    function createPanel(props) {
        const panel = createTemporaryObject(panelComponent, testCase,
            Object.assign({width: 400, height: 400}, props))
        verify(waitForRendering(panel))
        return panel
    }

    function test_rendersActivatesAndEditsSearch() {
        const panel = createPanel({scenarioModel: [{name: "Scenario A", hash: "a", runCount: 3, lastPlayedMs: 1723200000000}]})
        const list = findChild(panel, "scenarioListView")
        const delegate = findChild(panel, "scenarioItem_0")
        verify(list !== null)
        verify(delegate !== null)
        compare(list.count, 1)

        activatedSpy.target = panel
        verify(activatedSpy.valid)
        mouseClick(delegate, delegate.width / 2, delegate.height / 2)
        wait(0)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], "a")
        compare(activatedSpy.signalArguments[0][1], "Scenario A")

        searchSpy.target = panel
        verify(searchSpy.valid)
        const field = findChild(panel, "scenarioSearchField")
        mouseClick(field)
        keyClick(Qt.Key_X)
        wait(0)
        compare(panel.searchText, "x")
        compare(searchSpy.count, 1)
        compare(searchSpy.signalArguments[0][0], "x")
    }
}
