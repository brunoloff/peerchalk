import QtQuick

// The lines of the connection map, under its cards: from each way of finding devices to this
// device (dashed when it does not answer), and from this device to each group of devices,
// solid blue when direct, dashed amber through a relay.
Canvas {
    id: lines

    required property var map // NetworkMap
    readonly property var watched: [map.wide, map.deviceCard.y, map.directCard.visible,
                                    map.directCard.height, map.relayCard.visible, map.relayCard.y]

    function stroke(context, from, to, color, width, dashed) {
        context.beginPath();
        context.setLineDash(dashed ? [5, 5] : []);
        context.strokeStyle = color;
        context.lineWidth = width;
        context.lineCap = "round";
        context.moveTo(from.x, from.y);
        context.lineTo(to.x, to.y);
        context.stroke();
    }

    function methodLine(context, index) {
        const down = map.methods[index].state === "unreachable";
        const start = map.methodCenter(index);
        const end = map.wide ? Qt.point(map.busX, start.y) : Qt.point(start.x, map.busY);
        stroke(context, start, end, down ? Theme.lineFaint : Theme.line, 2, down);
    }

    function groupCards() {
        return [map.directCard, map.relayCard].filter(card => card.visible);
    }

    function paintWide(context) {
        const count = map.methods.length;
        const device = map.deviceCard;
        const middle = device.y + device.height / 2;
        if (count > 0) {
            stroke(context, Qt.point(map.busX, map.methodCenter(0).y),
                   Qt.point(map.busX, map.methodCenter(count - 1).y), Theme.line, 2, false);
            stroke(context, Qt.point(map.busX, middle), Qt.point(device.x, middle), Theme.line, 2, false);
        }
        const branch = device.x + device.width + 36;
        for (const card of groupCards()) {
            const relayed = card.route === "relay";
            const color = relayed ? Theme.relay : Theme.accent;
            const y = card.y + 17;
            stroke(context, Qt.point(device.x + device.width, middle), Qt.point(branch, middle), Theme.line, 2, false);
            stroke(context, Qt.point(branch, middle), Qt.point(branch, y), color, 2.5, relayed);
            stroke(context, Qt.point(branch, y), Qt.point(card.x, y), color, 2.5, relayed);
        }
    }

    function paintTall(context) {
        const count = map.methods.length;
        const device = map.deviceCard;
        const center = map.width / 2;
        if (count > 1)
            stroke(context, Qt.point(map.methodCenter(0).x, map.busY),
                   Qt.point(map.methodCenter(count - 1).x, map.busY), Theme.line, 2, false);
        if (count > 0)
            stroke(context, Qt.point(center, map.busY), Qt.point(center, device.y), Theme.line, 2, false);
        const cards = groupCards();
        if (cards.length === 0)
            return;
        const split = device.y + device.height + 36;
        stroke(context, Qt.point(center, device.y + device.height), Qt.point(center, split), Theme.line, 2, false);
        for (const card of cards) {
            const relayed = card.route === "relay";
            const color = relayed ? Theme.relay : Theme.accent;
            const x = card.x + card.width / 2;
            stroke(context, Qt.point(center, split), Qt.point(x, split), color, 2.5, relayed);
            stroke(context, Qt.point(x, split), Qt.point(x, card.y), color, 2.5, relayed);
        }
    }

    onWatchedChanged: requestPaint()
    onPaint: {
        const context = getContext("2d");
        context.reset();
        for (let index = 0; index < map.methods.length; ++index)
            methodLine(context, index);
        if (map.wide)
            paintWide(context);
        else
            paintTall(context);
    }
}
