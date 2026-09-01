// Item-tree lookups shared by the tst_*.qml suites.
//
// TestCase.findChild doesn't reliably reach items nested under StackLayout pages,
// Repeater delegates or ListView delegates (confirmed by inspection: the target
// items exist and are visible, but findChild still returns null), so these walk
// the tree by hand instead.

function findByObjectName(root, name) {
    if (!root)
        return null;
    if (root.objectName === name)
        return root;
    for (const child of root.children || []) {
        const found = findByObjectName(child, name);
        if (found)
            return found;
    }
    return null;
}

function findByObjectNamePrefix(root, prefix, matches) {
    if (!root)
        return matches;
    if (root.objectName && root.objectName.startsWith(prefix))
        matches.push(root);
    for (const child of root.children || [])
        findByObjectNamePrefix(child, prefix, matches);
    return matches;
}

function findByText(root, text) {
    if (!root)
        return null;
    if (root.text === text)
        return root;
    for (const child of root.children || []) {
        const found = findByText(child, text);
        if (found)
            return found;
    }
    return null;
}
