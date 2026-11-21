pragma Singleton

import QtQuick

// Rounded line icons as SVG path data on a 24x24 grid, drawn by Icon.
QtObject {
    readonly property string pan: "M7.5 12V6.2a1.5 1.5 0 0 1 3 0V11M10.5 11V4.7a1.5 1.5 0 0 1 3 0V11M13.5 11V5.7a1.5 1.5 0 0 1 3 0V12M16.5 12V8.2a1.5 1.5 0 0 1 3 0V14c0 4-2.7 7-6.8 7h-1.1c-2.3 0-3.8-.8-5.1-2.4l-3.1-4a1.6 1.6 0 0 1 2.4-2.1l2.7 1.7"
    readonly property string pen: "M16.6 3.9a2.2 2.2 0 0 1 3.1 0l.4.4a2.2 2.2 0 0 1 0 3.1L8.6 18.9l-4.8 1.3 1.3-4.8zM14.5 6L18 9.5"
    readonly property string highlighter: "M14.8 3.8l5.4 5.4-8.6 8.6-5.4-5.4zM6.2 12.4L5 16.8 7.2 19l4.4-1.2M5.4 18.6L3.8 20.2H9"
    readonly property string eraser: "M8.5 19.5l-4.6-4.6a1.8 1.8 0 0 1 0-2.6l8.4-8.4a1.8 1.8 0 0 1 2.6 0l5.2 5.2a1.8 1.8 0 0 1 0 2.6l-7.7 7.8zM8.2 8l7.8 7.8M12.4 19.5H21"
    readonly property string undo: "M8.5 5L4 9.5 8.5 14M4.5 9.5H14a5.5 5.5 0 0 1 0 11h-3"
    readonly property string fit: "M4 8.5V6a2 2 0 0 1 2-2h2.5M15.5 4H18a2 2 0 0 1 2 2v2.5M20 15.5V18a2 2 0 0 1-2 2h-2.5M8.5 20H6a2 2 0 0 1-2-2v-2.5"
    readonly property string equation: "M17.5 5H6.5l6.5 7-6.5 7h11"
    readonly property string text: "M5 7V5h14v2M12 5v14M9 19h6"
    readonly property string bold: "M7 5h6a3.5 3.5 0 0 1 0 7H7zM7 12h7a3.5 3.5 0 0 1 0 7H7z"
    readonly property string italic: "M10 5h7M7 19h7M14 5l-4 14"
    readonly property string trash: "M5 7h14M10 7V5h4v2M7 7l1 12h8l1-12M10.5 11v4M13.5 11v4"
    readonly property string exportFile: "M12 4v10.5M7.5 10L12 14.5 16.5 10M5 17.5V19a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-1.5"
    readonly property string plus: "M12 5v14M5 12h14"
    readonly property string minus: "M5 12h14"
    readonly property string pin: "M9 3.5h6M10 3.5v6.2L7 13v1.5h10V13l-3-3.3V3.5M12 14.5v6"
    readonly property string close: "M6 6l12 12M18 6L6 18"
    readonly property string chevronLeft: "M14.5 5.5L8 12l6.5 6.5"
    readonly property string more: "M5 12h.01M12 12h.01M19 12h.01"
    readonly property string pages: "M7 3h7l4 4v14H7zM14 3v4h4M10 12h5M10 16h5"
    readonly property string share: "M12 15V4M8 7.5l4-4 4 4M5 12.5V18a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-5.5"
    readonly property string check: "M5 12.5l5 5L19.5 7"
    readonly property string arrowRight: "M5 12h14M13 6l6 6-6 6"
    readonly property string download: "M12 3v12M7 10l5 5 5-5M5 21h14"
    readonly property string peers: "M5 9.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM19 2.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM19 16.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM7.3 11l9.4-4.8M7.3 13l9.4 4.8M19 7.5v9"
    readonly property string browser: "M5 4h14a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2zM3 8h18M8 21h8"
    readonly property string bolt: "M13 2L4 14h7l-1 8 9-12h-7z"
    readonly property string relay: "M4 12h5M15 12h5M9 8h6v8H9z"
    // A hand-drawn arrow curving down and to the right.
    readonly property string scribbleArrow: "M2 4c10-1 17 5 18 16M20 20l-5-4M20 20l1.5-6.5"
}
