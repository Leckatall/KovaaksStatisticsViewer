import QtQuick

QtObject {
    id: model

    property var root: null
    property string selectedNodeId: ""
    property int treeRevision: 0
    readonly property var exampleNames: [
        qsTr("1. Empty"),
        qsTr("2. Rolling accuracy"),
        qsTr("3. Rate division"),
        qsTr("4. Across runs"),
        qsTr("5. Deep nesting")
    ]
    readonly property var primitiveMetrics: ["Score", "Shots", "Hits", "Kills", "Dmg"]
    readonly property var nodeKinds: ["primitive", "constant", "add", "subtract", "multiply", "divide", "runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns"]

    property int nextId: 1

    function touch() {
        treeRevision += 1
    }

    function makeNode(kind) {
        const node = { id: "node-" + nextId++, kind: kind }
        if (kind === "primitive") {
            node.metric = "Score"
        } else if (kind === "constant") {
            node.value = 1
        } else if (isBinary(kind)) {
            node.left = null
            node.right = null
        } else {
            node.input = null
            if (kind === "rollingMean") node.window = 10
            if (kind === "averageAcrossRuns") node.selection = { kind: "recentRuns", count: 5 }
        }
        return node
    }

    function isBinary(kind) {
        return kind === "add" || kind === "subtract" || kind === "multiply" || kind === "divide"
    }

    function findLocation(id, node, parent, slot) {
        if (!node) return null
        if (node.id === id) return { node: node, parent: parent, slot: slot }
        if (isBinary(node.kind)) {
            return findLocation(id, node.left, node, "left") || findLocation(id, node.right, node, "right")
        }
        return findLocation(id, node.input, node, "input")
    }

    function locationFor(id) {
        return findLocation(id, root, null, "root")
    }

    function nodeById(id) {
        const location = locationFor(id)
        return location ? location.node : null
    }

    function pathTo(id, node, path) {
        if (!node) return null
        const extended = path.concat([node])
        if (node.id === id) return extended
        if (isBinary(node.kind)) {
            return pathTo(id, node.left, extended) || pathTo(id, node.right, extended)
        }
        return pathTo(id, node.input, extended)
    }

    function ancestorChain(id) {
        return pathTo(id, root, []) || []
    }

    function selectionText(node) {
        return node.selection.kind === "recentRuns"
            ? qsTr("recent %1").arg(node.selection.count)
            : qsTr("top %1%").arg(node.selection.percent)
    }

    function describe(node) {
        if (!node) return "…"
        if (node.kind === "primitive") return node.metric
        if (node.kind === "constant") return String(node.value)
        if (isBinary(node.kind)) {
            const symbol = ({ add: "+", subtract: "−", multiply: "×", divide: "÷" })[node.kind]
            return describe(node.left) + " " + symbol + " " + describe(node.right)
        }
        const label = ({
            runningSum: "RunningSum", rollingMean: "RollingMean",
            projectedFinalValue: "ProjectedFinalValue", projectRateToFinal: "ProjectRateToFinal",
            averageAcrossRuns: "AverageAcrossRuns"
        })[node.kind]
        if (node.kind === "rollingMean") {
            return label + "(" + describe(node.input) + ", window: " + node.window + ")"
        }
        if (node.kind === "averageAcrossRuns") {
            return label + "(" + describe(node.input) + ", over: " + selectionText(node) + ")"
        }
        return label + "(" + describe(node.input) + ")"
    }

    function select(id) {
        selectedNodeId = id
    }

    function replaceChild(parentId, slot, kind) {
        const replacement = makeNode(kind)
        if (slot === "root") {
            root = replacement
        } else {
            const parent = nodeById(parentId)
            if (!parent) return
            parent[slot] = replacement
        }
        selectedNodeId = replacement.id
        touch()
    }

    function deleteNode(id) {
        const location = locationFor(id)
        if (!location) return
        if (!location.parent) {
            root = null
            selectedNodeId = ""
        } else {
            location.parent[location.slot] = null
            selectedNodeId = location.parent.id
        }
        touch()
    }

    function wrapSelected(kind) {
        const location = locationFor(selectedNodeId)
        if (!location) return
        const wrapper = makeNode(kind)
        if (isBinary(kind)) {
            wrapper.left = location.node
        } else {
            wrapper.input = location.node
        }
        if (!location.parent) {
            root = wrapper
        } else {
            location.parent[location.slot] = wrapper
        }
        selectedNodeId = wrapper.id
        touch()
    }

    function changeBinaryOperator(id, kind) {
        const node = nodeById(id)
        if (!node || !isBinary(kind)) return
        node.kind = kind
        touch()
    }

    function updateField(id, field, value) {
        const node = nodeById(id)
        if (!node) return
        if (node.kind === "averageAcrossRuns" && (field === "count" || field === "percent")) {
            node.selection[field] = value
        } else {
            node[field] = value
        }
        touch()
    }

    function changeSelectionKind(id, kind) {
        const node = nodeById(id)
        if (!node || node.kind !== "averageAcrossRuns") return
        node.selection = kind === "recentRuns" ? { kind: kind, count: 5 } : { kind: kind, percent: 10 }
        touch()
    }

    function loadExample(index) {
        nextId = 1
        if (index === 0) {
            root = null
        } else if (index === 1) {
            root = makeNode("rollingMean")
            root.window = 10
            root.input = makeNode("divide")
            root.input.left = makeNode("primitive")
            root.input.left.metric = "Hits"
            root.input.right = makeNode("primitive")
            root.input.right.metric = "Shots"
        } else if (index === 2) {
            root = makeNode("divide")
            root.left = makeNode("projectRateToFinal")
            root.left.input = makeNode("primitive")
            root.left.input.metric = "Dmg"
            root.right = makeNode("runningSum")
            root.right.input = makeNode("primitive")
            root.right.input.metric = "Shots"
        } else if (index === 3) {
            root = makeNode("averageAcrossRuns")
            root.selection = { kind: "topPercentile", percent: 10 }
            root.input = makeNode("divide")
            root.input.left = makeNode("primitive")
            root.input.left.metric = "Hits"
            root.input.right = makeNode("primitive")
            root.input.right.metric = "Shots"
        } else {
            root = makeNode("subtract")
            root.left = makeNode("rollingMean")
            root.left.window = 3
            root.left.input = makeNode("projectedFinalValue")
            root.left.input.input = makeNode("primitive")
            root.left.input.input.metric = "Score"
            root.right = makeNode("primitive")
            root.right.metric = "Score"
        }
        selectedNodeId = root ? root.id : ""
        touch()
    }

    Component.onCompleted: loadExample(0)
}
