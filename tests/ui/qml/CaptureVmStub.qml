import QtQuick

QtObject {
    property bool previewOpen: false
    property string previewState: "ready"
    property string previewTargetName: ""
    property var previewImage: null
    property int previewImageWidth: previewImage ? previewImage.width : 0
    property int previewImageHeight: previewImage ? previewImage.height : 0
    property bool hasReference: true
    property int outputWidth: 0
    property int outputHeight: 0
    property int percent: 200
    property bool includeSummary: true
    property bool summaryAvailable: false
    property bool isGraph: true
    property bool keepAspect: true
    property string widthText: ""
    property string heightText: ""
    property string widthError: ""
    property string heightError: ""
    property string previewError: ""
    property bool canOutput: false
    property bool optionsLocked: false
    property string suggestedFileName: "capture.png"
    property string feedbackKind: "none"
    property string feedbackText: ""

    property bool beginExportResult: true
    property var calls: []

    signal previewClosed()

    function record(name, args) { calls = calls.concat([{name: name, args: args}]) }
    function callsNamed(name) { return calls.filter(c => c.name === name) }

    function useTheme(theme, font) { record("useTheme", [theme, font]) }
    function quickCopy(key, canvas) { record("quickCopy", [key, canvas]) }
    function openPreview(key, canvas) { record("openPreview", [key, canvas]) }
    function closePreview() { record("closePreview", []) }
    function setPercent(value) { record("setPercent", [value]) }
    function setIncludeSummary(include) { record("setIncludeSummary", [include]) }
    function setKeepAspect(keep) { record("setKeepAspect", [keep]) }
    function commitWidth(text) { record("commitWidth", [text]) }
    function commitHeight(text) { record("commitHeight", [text]) }
    function retryRender() { record("retryRender", []) }
    function copyPreview() { record("copyPreview", []) }
    function beginExport() { record("beginExport", []); return beginExportResult }
    function cancelExport() { record("cancelExport", []) }
    function confirmExport(url) { record("confirmExport", [url]) }
    function dismissFeedback() { record("dismissFeedback", []) }
}
