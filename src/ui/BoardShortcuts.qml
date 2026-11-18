import QtQuick

// The keyboard shortcuts of a board.
Item {
    id: shortcuts

    required property BoardCanvas canvas

    Shortcut {
        sequences: [StandardKey.Undo]
        onActivated: shortcuts.canvas.undo()
    }

    Shortcut {
        sequence: "P"
        onActivated: shortcuts.canvas.tool = BoardCanvas.Pen
    }

    Shortcut {
        sequence: "M"
        onActivated: shortcuts.canvas.tool = BoardCanvas.Highlighter
    }

    Shortcut {
        sequence: "T"
        onActivated: shortcuts.canvas.tool = BoardCanvas.Text
    }

    Shortcut {
        sequence: "E"
        onActivated: shortcuts.canvas.tool = BoardCanvas.Eraser
    }

    Shortcut {
        sequence: "H"
        onActivated: shortcuts.canvas.tool = BoardCanvas.Pan
    }

    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+="]
        onActivated: shortcuts.canvas.zoomBy(1.25)
    }

    Shortcut {
        sequences: [StandardKey.ZoomOut]
        onActivated: shortcuts.canvas.zoomBy(1 / 1.25)
    }

    Shortcut {
        sequence: "Ctrl+0"
        onActivated: shortcuts.canvas.resetView()
    }

    Shortcut {
        sequence: "Shift+1"
        onActivated: shortcuts.canvas.fitContent()
    }
}
