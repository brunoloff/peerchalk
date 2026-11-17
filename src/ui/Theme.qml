pragma Singleton

import QtQuick

// Design tokens from the "Paper" direction.
QtObject {
    readonly property color background: "#F7F6F2"
    readonly property color boardBackground: "#FBFAF7"
    readonly property color gridDot: "#D6D2C9"
    readonly property color surface: "#FFFFFF"
    readonly property color ink: "#1C1B19"
    readonly property color muted: "#5E5B55"
    readonly property color border: "#E4E1DA"
    readonly property color accent: "#2F5BEA"
    readonly property color accentSoft: "#EDF1FD"
    readonly property color hover: "#F3F1EC"

    readonly property string bodyFont: "Inter"
    readonly property string headingFont: "Space Grotesk"

    readonly property int panelRadius: 14
    readonly property int controlRadius: 10
    readonly property int gap: 16

    readonly property var penColors: ["#1C1B19", "#2F5BEA", "#C2410C", "#15803D", "#7E22CE"]
    readonly property var penWidths: [2, 4, 8]
}
