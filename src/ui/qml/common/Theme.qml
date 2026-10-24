pragma Singleton
import QtQuick

QtObject {
    // The app's complete palette. Every top-level window must set `palette: Theme.palette`:
    // a role a window leaves unset falls back to the Windows theme, which derives colours
    // from the user's accent and gives Switch/CheckBox their own per-control palette.
    readonly property Palette palette: Palette {
        window: "#1E1E1E"
        windowText: "#FFFFFF"
        base: "#2D2D2D"
        alternateBase: "#343434"
        text: "#FFFFFF"
        button: "#3C3C3C"
        buttonText: "#FFFFFF"
        brightText: "#65BCFF"
        light: "#787878"
        midlight: "#5A5A5A"
        mid: "#282828"
        dark: "#1E1E1E"
        shadow: "#000000"
        highlight: "#00838F"
        highlightedText: "#FFFFFF"
        link: "#65BCFF"
        linkVisited: "#32A6FF"
        toolTipBase: "#3C3C3C"
        toolTipText: "#D4D4D4"
        placeholderText: "#80FFFFFF"
        accent: "#00BCD4"

        disabled {
            windowText: "#9D9D9D"
            base: "#1E1E1E"
            text: "#9D9D9D"
            buttonText: "#9D9D9D"
        }
    }

    readonly property color errorText: "#E57373"
    readonly property color error: "#E53935"
    readonly property color warning: "#FFA726"
    readonly property color attention: "#FFB74D"
    readonly property color success: "#4CAF50"

    readonly property int toolTipDelay: 400
    readonly property int shortToolTipDelay: 300

    readonly property real groupTintAlpha: 0.22

    function classificationColor(classification) {
        return ({
            Trackable: success,
            Incomplete: warning,
            Invalid: error,
            Unsupported: "#9E9E9E"
        })[classification] || "#9E9E9E"
    }

    // `color` as text on `background`: the colour itself when it reads (WCAG 4.5:1),
    // otherwise the first 5% step of a blend toward `fallback` (the palette's text
    // colour for that background) that does. Works for light and dark palettes alike.
    function readableOn(color, background, fallback) {
        if (String(color).length === 0) return fallback
        const linear = c => c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)
        const luminance = c => 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b)
        const contrast = (a, b) => {
            const la = luminance(a), lb = luminance(b)
            return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05)
        }
        const base = Qt.color(color), bg = Qt.color(background), to = Qt.color(fallback)
        for (let step = 0; step <= 20; ++step) {
            const t = step / 20
            const mixed = Qt.rgba(base.r + (to.r - base.r) * t, base.g + (to.g - base.g) * t,
                                  base.b + (to.b - base.b) * t, 1)
            if (contrast(mixed, bg) >= 4.5) return mixed
        }
        return fallback
    }

    function isLightFill(color) {
        const c = Qt.color(color)
        return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) > 0.55
    }
}
