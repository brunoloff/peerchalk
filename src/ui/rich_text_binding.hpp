#pragma once

#include "board/rich_text.hpp"
#include "math/math_engine.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTextCursor>
#include <QTextObjectInterface>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace peerchalk {

class BoardCanvas;

// Keeps a text box on the board and the document of its TextEdit equal, in both directions.
// Equations occupy space in the text as empty objects; `equations` says where to draw them.
class RichTextBinding : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
    QML_ELEMENT
    Q_PROPERTY(peerchalk::BoardCanvas *canvas MEMBER canvas_ WRITE setCanvas)
    Q_PROPERTY(QString boxId MEMBER boxId_ WRITE setBoxId)
    Q_PROPERTY(QQuickTextDocument *document WRITE setDocument)
    Q_PROPERTY(qreal fontSize MEMBER fontSize_ WRITE setFontSize)
    // Each entry has position, x, y (top left), tex, display and failed.
    Q_PROPERTY(QVariantList equations READ equations NOTIFY equationsChanged)
    // The equation the text cursor has stopped at, or -1, and where the cursor goes back to
    // when it is done there: the side it came from.
    Q_PROPERTY(int focusedEquation READ focusedEquation NOTIFY focusedEquationChanged)
    Q_PROPERTY(int equationReturnPosition READ equationReturnPosition
                   NOTIFY focusedEquationChanged)
    Q_PROPERTY(QString focusedEquationTex READ focusedEquationTex NOTIFY focusedEquationChanged)
    Q_PROPERTY(bool focusedEquationDisplay READ focusedEquationDisplay
                   NOTIFY focusedEquationChanged)

  public:
    explicit RichTextBinding(QObject *parent = nullptr);

    void setCanvas(BoardCanvas *canvas);
    void setBoxId(const QString &id);
    void setDocument(QQuickTextDocument *document);
    void setTextDocument(QTextDocument *document);
    void setFontSize(qreal size);
    [[nodiscard]] QVariantList equations() const { return equations_; }
    [[nodiscard]] int focusedEquation() const;
    [[nodiscard]] int equationReturnPosition() const;
    [[nodiscard]] QString focusedEquationTex() const;
    [[nodiscard]] bool focusedEquationDisplay() const;

    // Applies to the range, or to the next typed text when the range is empty.
    Q_INVOKABLE void toggleBold(int start, int end);
    Q_INVOKABLE void toggleItalic(int start, int end);
    Q_INVOKABLE [[nodiscard]] bool isBold(int position) const;
    Q_INVOKABLE [[nodiscard]] bool isItalic(int position) const;
    Q_INVOKABLE void insertEquation(int position, const QString &tex, bool display);
    // A display equation is a block: it gets a line of its own, centered. replaceEquation
    // returns the equation's new position, or -1 if there is none at position.
    Q_INVOKABLE int replaceEquation(int position, const QString &tex, bool display);
    // An arrow key at cursor; direction is -1 or 1. An equation in the way stops the cursor:
    // it becomes focused and the cursor stays. The next arrow leaves it on that arrow's
    // side. Returns the new cursor position, or -1 if the key should act as usual.
    Q_INVOKABLE int moveAcrossEquation(int cursor, int direction);
    Q_INVOKABLE void focusEquation(int position, bool returnAfter);
    Q_INVOKABLE void clearEquationFocus();
    Q_INVOKABLE void removeFocusedEquation();
    // Sends anything typed so far to the board and ends the undo step.
    Q_INVOKABLE void flush();

    QSizeF intrinsicSize(QTextDocument *document, int position, const QTextFormat &format) override;
    void drawObject(QPainter *, const QRectF &, QTextDocument *, int,
                    const QTextFormat &) override {}

  signals:
    void equationsChanged();
    void focusedEquationChanged();
    void cursorRequested(int position);

  private:
    struct Metrics {
        double width{1.0};
        double height{0.8};
        double depth{0.2};
        bool ready{false};
        bool failed{false};
    };
    enum class Style { Bold, Italic };

    void attach();
    void documentEdited(int position, int removed, int added);
    void pushLocalChanges();
    void syncFromBoard();
    void commit(const RichText &body);
    void toggle(Style style, int start, int end);
    void applyPendingStyle(RichText &body) const;
    [[nodiscard]] std::optional<int> convertTypedEquations(RichText &body) const;
    [[nodiscard]] bool hasStyle(Style style, int position) const;
    [[nodiscard]] const Metrics &metricsFor(const QString &tex, bool display);
    void metricsArrived(const QString &key, const MathFormula &formula);
    void updateLayoutInfo();
    void relayoutEquations();
    void checkEquationFocus();

    QPointer<BoardCanvas> canvas_;
    QString boxId_;
    QPointer<QTextDocument> document_;
    qreal fontSize_{20.0};
    QVariantList equations_;
    QHash<QString, Metrics> metrics_;
    std::optional<QTextCursor> equationFocus_; // follows the equation through edits
    bool returnAfter_{false};
    int reportedFocus_{-1};
    int equationRevision_{0};
    QTimer layoutTimer_;
    QTimer undoTimer_;
    bool applying_{false};
    bool localDirty_{false};
    int editStart_{-1}; // the range edited since the last push, while localDirty_
    int editEnd_{-1};
    std::optional<bool> nextBold_;
    std::optional<bool> nextItalic_;
};

} // namespace peerchalk
