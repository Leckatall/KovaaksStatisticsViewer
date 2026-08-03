import QtQuick
import QtGraphs

LineSeries {
    id: line
    capStyle: Qt.RoundCap
            joinStyle: Qt.RoundJoin
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
