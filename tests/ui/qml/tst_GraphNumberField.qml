import QtQuick
import QtQuick.Layouts
import QtTest
import "../../../src/ui/qml/graph"

Item {
    id: root
    width: 640
    height: 300

    Component {
        id: fieldComponent
        GridLayout {
            width: 600
            columns: 2

            GraphNumberField {
                id: field
                Layout.fillWidth: true
                fieldName: "number"
                label: qsTr("Minimum")
                shown: "10"
            }
            Item { Layout.fillWidth: true }

            property alias field: field
        }
    }

    TestCase {
        name: "GraphNumberFieldTest"
        when: windowShown

        function test_layoutSettles_data() {
            return [
                {tag: "withUnit", unit: "days"},
                {tag: "withoutUnit", unit: ""}
            ]
        }

        function test_layoutSettles(data) {
            failOnWarning(/recursive rearrange|binding loop/i)
            const layout = createTemporaryObject(fieldComponent, root)
            verify(layout !== null)
            const field = layout.field
            field.unit = data.unit
            const unit = findChild(field, "number_unit")
            verify(unit !== null)
            verify(waitForRendering(layout))

            for (const width of [160, 240, 600, 320, 600]) {
                layout.width = width
                verify(waitForRendering(layout))
                verify(field.input.width > 0)
                verify(field.input.width <= field.width)
                if (data.unit !== "") {
                    const unitX = unit.mapToItem(field, 0, 0).x
                    verify(unitX >= field.input.width)
                    verify(unitX + unit.width <= field.width)
                }
                compare(field.input.text, "10")
            }
        }
    }
}
