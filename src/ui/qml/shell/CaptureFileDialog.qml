import QtQuick
import QtQuick.Dialogs
import QtCore

FileDialog {
    id: root

    fileMode: FileDialog.SaveFile
    nameFilters: [qsTr("PNG image (*.png)")]
    defaultSuffix: "png"
    currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)

    function openFor(fileName: string) {
        root.currentFile = root.currentFolder + "/" + fileName
        root.open()
    }
}
