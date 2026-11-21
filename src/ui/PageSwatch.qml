import QtQuick

// A tiny sheet showing a page background: "blank", "ruled", "squared" or "dotted".
Canvas {
    id: swatch

    property string pattern: "blank"
    property real spacing: 5

    implicitWidth: 22
    implicitHeight: 28
    onPatternChanged: requestPaint()
    onSpacingChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.fillStyle = "#FFFFFF";
        ctx.fillRect(0, 0, width, height);
        if (pattern === "ruled" || pattern === "squared") {
            ctx.fillStyle = pattern === "ruled" ? "#9DB3F0" : "#C9D4F5";
            for (let y = spacing; y < height; y += spacing)
                ctx.fillRect(0, Math.round(y), width, 1);
            if (pattern === "squared")
                for (let x = spacing; x < width; x += spacing)
                    ctx.fillRect(Math.round(x), 0, 1, height);
        } else if (pattern === "dotted") {
            ctx.fillStyle = "#8F8A80";
            for (let y = spacing; y < height; y += spacing)
                for (let x = spacing; x < width; x += spacing)
                    ctx.fillRect(Math.round(x) - 0.5, Math.round(y) - 0.5, 1.2, 1.2);
        }
        ctx.strokeStyle = "#B9B4AA";
        ctx.lineWidth = 1;
        ctx.strokeRect(0.5, 0.5, width - 1, height - 1);
    }
}
