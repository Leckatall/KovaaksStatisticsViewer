import QtQuick
import QtGraphs

SplineSeries {
    id: line
    capStyle: Qt.RoundCap
    required property var line_model
    required property int xIndex
    required property int yIndex

    XYModelMapper {
        series: line
        model: line.line_model
        xSection: line.xIndex
        ySection: line.yIndex
    }
}
