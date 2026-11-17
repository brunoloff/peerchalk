pragma Singleton

import QtQuick

// Text formatting shared by the screens.
QtObject {
    // "just now", "5 min ago", "3 h ago", "yesterday", "4 days ago", or a date.
    function relativeTime(date: date): string {
        const seconds = (Date.now() - date.getTime()) / 1000;
        if (isNaN(seconds))
            return qsTr("never");
        if (seconds < 60)
            return qsTr("just now");
        if (seconds < 3600)
            return qsTr("%1 min ago").arg(Math.floor(seconds / 60));
        if (seconds < 86400)
            return qsTr("%1 h ago").arg(Math.floor(seconds / 3600));
        if (seconds < 2 * 86400)
            return qsTr("yesterday");
        if (seconds < 7 * 86400)
            return qsTr("%1 days ago").arg(Math.floor(seconds / 86400));
        return date.toLocaleDateString(Qt.locale(), Locale.ShortFormat);
    }

    // The first letter of a name, upper case, for avatars.
    function initial(name: string): string {
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?";
    }

    // A stable avatar colour index for someone identified by a hex key.
    function keyColorIndex(keyHex: string, count: int): int {
        return parseInt(keyHex.slice(0, 2), 16) % count;
    }
}
