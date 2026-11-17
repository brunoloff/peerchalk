pragma Singleton

import QtQuick

// Stroke icons as SVG path data on a 24x24 grid, drawn by Icon.
QtObject {
    readonly property string pan: "M8 13V5a1.5 1.5 0 0 1 3 0v6M11 10V4a1.5 1.5 0 0 1 3 0v6M14 10V5.5a1.5 1.5 0 0 1 3 0V14a6 6 0 0 1-6 6h-1a6 6 0 0 1-5-3l-2.5-4a1.5 1.5 0 0 1 2.5-1.5L8 13"
    readonly property string pen: "M4 20l4-1 11-11-3-3L5 16zM14 6l3 3"
    readonly property string highlighter: "M9 15l-4 4h5l2-2M9 15l8-8 3 3-8 8z"
    readonly property string eraser: "M7 20h12M5 15l9-9 5 5-8 8H8z"
    readonly property string undo: "M9 14L4 9l5-5M4 9h10a6 6 0 0 1 0 12h-3"
    readonly property string fit: "M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"
    readonly property string plus: "M12 5v14M5 12h14"
    readonly property string minus: "M5 12h14"
    readonly property string pin: "M9 4h6l-1 6 3 3H7l3-3zM12 13v7"
    readonly property string close: "M6 6l12 12M18 6L6 18"
    readonly property string chevronLeft: "M15 6l-6 6 6 6"
    readonly property string more: "M5 12h.01M12 12h.01M19 12h.01"
    readonly property string share: "M4 12v7a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-7M12 3v12M8 7l4-4 4 4"
    readonly property string check: "M5 12l5 5 9-10"
}
