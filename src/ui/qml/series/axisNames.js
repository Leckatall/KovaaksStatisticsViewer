.pragma library

function message(code) {
    switch (code) {
    case "axisNameBlank":
        return qsTr("Enter an axis name.")
    case "axisNameDuplicate":
        return qsTr("Another axis already has this name.")
    default:
        return ""
    }
}
