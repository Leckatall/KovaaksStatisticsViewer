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
        xSection: line.xIndex
        ySection: line.yIndex
        // model is assigned last: setting it triggers an immediate
        // initializeXYFromModel(), so xSection/ySection must already be
        // valid or it logs "Invalid X/Y coordinate index" warnings.
        model: line.line_model
    }
}
