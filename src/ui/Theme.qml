pragma Singleton

import QtQuick

// Design tokens from the "Paper" direction.
QtObject {
    readonly property color background: "#F7F6F2"
    readonly property color boardBackground: "#FBFAF7"
    readonly property color gridDot: "#D6D2C9"
    // Page boards: the desk the pages lie on, and its dots.
    readonly property color desk: "#ECEAE4"
    readonly property color deskDot: "#DCD8CF"
    readonly property color accentInk: "#1E40AF" // accent text on accentSoft
    readonly property color surface: "#FFFFFF"
    readonly property color ink: "#1C1B19"
    readonly property color muted: "#5E5B55"
    readonly property color border: "#E4E1DA"
    readonly property color accent: "#2F5BEA"
    readonly property color accentSoft: "#EDF1FD"
    readonly property color hover: "#F3F1EC"
    // Text pressed into the paper: a pale fill under a darker top edge.
    readonly property color engraved: "#CBC6BC"
    readonly property color engravedEdge: "#A8A399"

    readonly property string bodyFont: "Inter"
    readonly property string headingFont: "Space Grotesk"

    readonly property int panelRadius: 14
    readonly property int controlRadius: 10
    readonly property int gap: 16

    // Eight hue columns (red, orange, yellow, green, teal, blue, purple, pink) under a row
    // of greys; rows go from pale (good for the highlighter) to deep. The first is the
    // default pen colour.
    readonly property int penColorColumns: 8
    readonly property var penColors: [
        "#1C1B19", "#333333", "#525252", "#737373", "#A3A3A3", "#C4C4C4", "#E5E5E5", "#FFFFFF",
        "#FECACA", "#FED7AA", "#FEF08A", "#BBF7D0", "#99F6E4", "#BFDBFE", "#E9D5FF", "#FBCFE8",
        "#F87171", "#FB923C", "#FACC15", "#4ADE80", "#2DD4BF", "#60A5FA", "#C084FC", "#F472B6",
        "#DC2626", "#EA580C", "#CA8A04", "#16A34A", "#0D9488", "#2563EB", "#9333EA", "#DB2777",
        "#991B1B", "#9A3412", "#854D0E", "#166534", "#115E59", "#1E40AF", "#6B21A8", "#9D174D"
    ]
    // Avatar colours for people (index from the profile, or from the key for others).
    readonly property var personColors: ["#2F5BEA", "#C2410C", "#15803D", "#7E22CE", "#0F766E"]
    // Marker colours for projects.
    readonly property var projectColors: ["#2F5BEA", "#E8772E", "#2E9467", "#7E22CE", "#0F766E", "#C2410C"]
    readonly property string monoFont: "JetBrains Mono"
    readonly property color success: "#166534"
    readonly property color successSoft: "#DCFCE7"
    readonly property color warningSoft: "#FFF7ED"
    readonly property color danger: "#B91C1C"
    readonly property var penWidths: [2, 4, 8]
    // Network status: servers that answer or not, and connections direct or relayed.
    readonly property color working: "#2E7D4F"
    readonly property color workingInk: "#2E6B45"
    readonly property color unreachable: "#A63D2F"
    readonly property color quiet: "#6B675F"    // secondary text that still reads at 11px
    readonly property color line: "#8A8378"     // lines on the connection map
    readonly property color lineFaint: "#C9C3B8"
    readonly property color directSoft: "#F3F6FE"
    readonly property color directBorder: "#C9D4F8"
    readonly property color relay: "#A15C00"
    readonly property color relayInk: "#8A4E00"
    readonly property color relaySoft: "#FFF6E8"
    readonly property color relayBorder: "#E2C08E"
}
