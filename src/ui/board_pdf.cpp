#include "ui/board_pdf.hpp"

#include "board/stroke_mesh.hpp"
#include "ui/equation_geometry.hpp"
#include "ui/rich_text_document.hpp"
#include "ui/scene_nodes.hpp"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QFontMetricsF>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPdfWriter>
#include <QTextDocument>
#include <QTextObjectInterface>

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace peerchalk {
namespace {

constexpr qreal pixelsPerInch = 96.0;
constexpr qreal pointsPerPixel = 72.0 / pixelsPerInch;
constexpr qreal maxPagePoints = 14400.0; // 200 inches, PDF's usual page limit
const QColor failedFormulaColor(0xB4, 0x23, 0x18);

qreal marginPixels() { return pdfMarginCm / 2.54 * pixelsPerInch; }

QColor fromRgba(std::uint32_t rgba) {
    return QColor(int(rgba >> 24), int(rgba >> 16 & 0xff), int(rgba >> 8 & 0xff), int(rgba & 0xff));
}

QPainterPath formulaPath(const MathFormula &formula, const MathRun &run) {
    QPainterPath path;
    path.setFillRule(Qt::WindingFill); // one fill, so no seams between triangles
    const auto point = [&](quint32 index) {
        return QPointF(formula.vertices[index * 2], formula.vertices[index * 2 + 1]);
    };
    for (quint32 i = run.firstIndex; i + 2 < run.firstIndex + run.indexCount; i += 3) {
        path.addPolygon(QPolygonF({point(formula.indices[i]), point(formula.indices[i + 1]),
                                   point(formula.indices[i + 2]), point(formula.indices[i])}));
    }
    return path;
}

} // namespace

// Draws equations where a text box's layout put them.
class PdfEquations : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

  public:
    PdfEquations(const BoardPdfInput &input, const TextBox &box) : input_(input), box_(box) {}

    QSizeF intrinsicSize(QTextDocument *document, int, const QTextFormat &format) override {
        const MathFormula *formula = find(format);
        const qreal xHeight = QFontMetricsF(document->defaultFont()).xHeight();
        if (formula == nullptr)
            return QFontMetricsF(failedFont()).size(0, tex(format)) + QSizeF(4.0, 0.0);
        return equationObjectSize(formula->width, formula->height, formula->depth, box_.fontSize,
                                  xHeight);
    }

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *document, int,
                    const QTextFormat &format) override {
        const MathFormula *formula = find(format);
        painter->save();
        if (formula == nullptr) {
            painter->setFont(failedFont());
            painter->setPen(failedFormulaColor);
            painter->drawText(rect, Qt::AlignCenter, tex(format));
        } else {
            const qreal xHeight = QFontMetricsF(document->defaultFont()).xHeight();
            painter->translate(equationOrigin(rect, formula->height, box_.fontSize, xHeight));
            painter->scale(box_.fontSize, box_.fontSize);
            painter->setPen(Qt::NoPen);
            for (const MathRun &run : formula->runs)
                painter->fillPath(formulaPath(*formula, run), fromRgba(run.rgba));
        }
        painter->restore();
    }

  private:
    [[nodiscard]] static QString tex(const QTextFormat &format) {
        return format.stringProperty(equationTexProperty);
    }
    [[nodiscard]] QFont failedFont() const {
        QFont font(QStringLiteral("monospace"));
        font.setPixelSize(std::max(1, int(box_.fontSize * 0.8)));
        return font;
    }
    [[nodiscard]] const MathFormula *find(const QTextFormat &format) const {
        RichUnit unit = equationUnit(tex(format).toStdString(),
                                     format.boolProperty(equationDisplayProperty));
        const auto found = input_.formulas.constFind(pdfFormulaKey(unit, box_.argb));
        return found != input_.formulas.cend() && found->error.isEmpty() ? &*found : nullptr;
    }

    const BoardPdfInput &input_;
    const TextBox &box_;
};

namespace {

// A text box laid out as TextEdit lays it out on screen.
struct LaidOutBox {
    const TextBox *box;
    std::unique_ptr<PdfEquations> equations;
    std::unique_ptr<QTextDocument> document;
};

LaidOutBox layOut(const BoardPdfInput &input, const TextBox &box) {
    LaidOutBox out{&box, std::make_unique<PdfEquations>(input, box),
                   std::make_unique<QTextDocument>()};
    QFont font(input.fontFamily);
    font.setPixelSize(std::max(1, int(std::lround(box.fontSize))));
    out.document->setDefaultFont(font);
    out.document->setDocumentMargin(0.0);
    QTextOption option = out.document->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    out.document->setDefaultTextOption(option);
    out.document->setTextWidth(box.width);
    out.document->documentLayout()->registerHandler(equationObjectType, out.equations.get());
    applyRichText(*out.document, box.body);
    return out;
}

std::optional<QRectF> contentBounds(const Board &board, const std::vector<LaidOutBox> &boxes) {
    std::optional<QRectF> bounds;
    const auto add = [&](const QRectF &rect) { bounds = bounds ? bounds->united(rect) : rect; };
    for (const Stroke &stroke : board.strokes()) {
        if (const std::optional<Bounds> b = stroke.bounds())
            add(QRectF(QPointF(b->left, b->top), QPointF(b->right, b->bottom)));
    }
    for (const LaidOutBox &laidOut : boxes) {
        const QSizeF size = laidOut.document->size();
        add(QRectF(laidOut.box->x, laidOut.box->y, laidOut.box->width, size.height()));
    }
    return bounds;
}

// Highlighters of one colour are filled together, so that overlaps do not darken, as on screen.
void drawHighlighters(QPainter &painter, const std::vector<Stroke> &strokes) {
    std::map<QRgb, QPainterPath> byColour;
    for (const Stroke &stroke : strokes) {
        if (stroke.kind() != StrokeKind::Highlighter || stroke.points().empty())
            continue;
        QPainterPath line(QPointF(stroke.points().front().x, stroke.points().front().y));
        for (const StrokePoint &point : stroke.points())
            line.lineTo(point.x, point.y);
        QPainterPathStroker stroker;
        stroker.setWidth(pointWidth(stroke, stroke.points().front()));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath &path = byColour[strokeColor(stroke, StrokeSelection::All).rgba()];
        path.setFillRule(Qt::WindingFill);
        path.addPath(stroker.createStroke(line));
    }
    for (const auto &[rgba, path] : byColour)
        painter.fillPath(path, QColor::fromRgba(rgba));
}

// Each segment is as wide as the stroke at its first point, with round ends, as on screen.
void drawPens(QPainter &painter, const std::vector<Stroke> &strokes) {
    for (const Stroke &stroke : strokes) {
        const std::vector<StrokePoint> &points = stroke.points();
        if (stroke.kind() == StrokeKind::Highlighter || points.empty())
            continue;
        QPen pen(strokeColor(stroke, StrokeSelection::All));
        pen.setCapStyle(Qt::RoundCap);
        if (points.size() == 1) {
            pen.setWidthF(pointWidth(stroke, points.front()));
            painter.setPen(pen);
            painter.drawPoint(QPointF(points.front().x, points.front().y));
            continue;
        }
        for (std::size_t i = 1; i < points.size(); ++i) {
            pen.setWidthF(pointWidth(stroke, points[i - 1]));
            painter.setPen(pen);
            painter.drawLine(QPointF(points[i - 1].x, points[i - 1].y),
                             QPointF(points[i].x, points[i].y));
        }
    }
}

void drawTextBoxes(QPainter &painter, const std::vector<LaidOutBox> &boxes) {
    for (const LaidOutBox &laidOut : boxes) {
        painter.save();
        painter.translate(laidOut.box->x, laidOut.box->y);
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Text, QColor::fromRgba(laidOut.box->argb));
        laidOut.document->documentLayout()->draw(&painter, context);
        painter.restore();
    }
}

} // namespace

QString pdfFormulaKey(const RichUnit &equation, std::uint32_t argb) {
    return QStringLiteral("%1|%2|%3")
        .arg(argb, 8, 16, QLatin1Char('0'))
        .arg(equation.displayMath ? 1 : 0)
        .arg(QString::fromStdString(equation.tex));
}

QStringList pdfFormulaKeys(const Board &board) {
    QStringList keys;
    for (const TextBox &box : board.textBoxes()) {
        for (const RichUnit &unit : box.body) {
            const QString key = unit.equation ? pdfFormulaKey(unit, box.argb) : QString();
            if (!key.isEmpty() && !keys.contains(key))
                keys.push_back(key);
        }
    }
    return keys;
}

QByteArray renderBoardPdf(const BoardPdfInput &input) {
    if (input.board == nullptr)
        throw std::invalid_argument("renderBoardPdf needs a board");
    std::vector<LaidOutBox> boxes;
    for (const TextBox &box : input.board->textBoxes())
        boxes.push_back(layOut(input, box));
    const std::optional<QRectF> content = contentBounds(*input.board, boxes);
    if (!content)
        throw std::invalid_argument("The board is empty");
    const qreal margin = marginPixels();
    const qreal largest = std::max(content->width(), content->height());
    const qreal limit = maxPagePoints / pointsPerPixel - 2.0 * margin;
    const qreal scale = largest > limit ? limit / largest : 1.0;
    const QSizeF page((content->width() * scale + 2.0 * margin) * pointsPerPixel,
                      (content->height() * scale + 2.0 * margin) * pointsPerPixel);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buffer);
    writer.setResolution(int(pixelsPerInch));
    writer.setPageSize(QPageSize(page, QPageSize::Point, QString(), QPageSize::ExactMatch));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle(input.title);
    writer.setCreator(QStringLiteral("PeerChalk"));
    QPainter painter;
    if (!painter.begin(&writer))
        throw std::runtime_error("The PDF could not be written");
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(margin, margin);
    painter.scale(scale, scale);
    painter.translate(-content->topLeft());
    drawHighlighters(painter, input.board->strokes());
    drawPens(painter, input.board->strokes());
    drawTextBoxes(painter, boxes);
    painter.end();
    return bytes;
}

} // namespace peerchalk

#include "board_pdf.moc"
