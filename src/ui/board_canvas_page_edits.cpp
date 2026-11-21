// Editing a page board's columns and items from QML (see BoardCanvas).
#include "ui/board_canvas.hpp"

#include "board/board_pages.hpp"
#include "library/blob_store.hpp"
#include "ui/pdf_renderer.hpp"

#include <QVariantMap>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr const char *backgroundNames[] = {"blank", "ruled", "squared", "dotted"};

std::optional<PageBackground> backgroundNamed(const QString &name) {
    for (std::size_t i = 0; i < std::size(backgroundNames); ++i) {
        if (name == QLatin1String(backgroundNames[i]))
            return static_cast<PageBackground>(i);
    }
    return std::nullopt;
}

std::string text(const QString &value) { return value.toStdString(); }

} // namespace

template <typename Edit> void BoardCanvas::editPages(Edit edit) {
    if (readOnly_ || !pageBoard_)
        return;
    pointerCancelled();
    edit(editableBoard().pages());
    strokesUpdated(); // the document's notification follows; the layout is needed now
}

void BoardCanvas::addPage(const QString &columnId, const QString &before) {
    editPages([&](BoardPages &pages) {
        pages.insertItem(text(columnId), text(before), ColumnItemKind::Page);
    });
}

void BoardCanvas::addSubheading(const QString &columnId, const QString &before,
                                const QString &title) {
    editPages([&](BoardPages &pages) {
        pages.insertItem(text(columnId), text(before), ColumnItemKind::Subheading, text(title));
    });
}

void BoardCanvas::removePageItem(const QString &columnId, const QString &itemId) {
    editPages([&](BoardPages &pages) { pages.removeItem(text(columnId), text(itemId)); });
}

void BoardCanvas::setSubheadingText(const QString &columnId, const QString &itemId,
                                    const QString &title) {
    editPages([&](BoardPages &pages) {
        pages.setSubheadingText(text(columnId), text(itemId), text(title));
    });
}

void BoardCanvas::addColumn(const QString &before) {
    editPages([&](BoardPages &pages) {
        const std::vector<ColumnSpec> &columns = pages.columns();
        if (columns.empty())
            return;
        const auto next = std::ranges::find(columns, text(before), &ColumnSpec::id);
        const ColumnSpec &like = next == columns.begin() ? columns.front() : *std::prev(next);
        const PaperSize paper = like.paper;
        const PageBackground background = like.background;
        pages.addColumn(text(before), paper, {}, background);
    });
}

void BoardCanvas::removeColumn(const QString &columnId) {
    editPages([&](BoardPages &pages) { pages.removeColumn(text(columnId)); });
}

void BoardCanvas::moveColumn(const QString &columnId, const QString &before) {
    editPages([&](BoardPages &pages) { pages.moveColumn(text(columnId), text(before)); });
}

void BoardCanvas::setColumnTitle(const QString &columnId, const QString &title) {
    editPages([&](BoardPages &pages) { pages.setColumnTitle(text(columnId), text(title)); });
}

void BoardCanvas::setColumnPaper(const QString &columnId, const QString &paper, bool landscape,
                                 bool allPages) {
    const std::optional<PaperSize> size = paperNamed(text(paper));
    if (!size) {
        qWarning("BoardCanvas: ignoring unknown paper size %s", qPrintable(paper));
        return;
    }
    editPages([&](BoardPages &pages) {
        pages.setColumnPaper(text(columnId), landscape ? peerchalk::landscape(*size) : *size,
                             allPages);
    });
}

void BoardCanvas::setColumnBackground(const QString &columnId, const QString &background) {
    const std::optional<PageBackground> pattern = backgroundNamed(background);
    if (!pattern) {
        qWarning("BoardCanvas: ignoring unknown page background %s", qPrintable(background));
        return;
    }
    editPages([&](BoardPages &pages) { pages.setColumnBackground(text(columnId), *pattern); });
}

QVariantList BoardCanvas::pageColumns() const {
    QVariantList list;
    if (!pageBoard_)
        return list;
    const std::vector<ColumnSpec> &columns = board().pages().columns();
    const PageLayout &layout = pageMap_.layout();
    for (std::size_t c = 0; c < layout.columns.size() && c < columns.size(); ++c) {
        const ColumnSpec &spec = columns[c];
        list.append(QVariantMap{
            {"columnId", QString::fromStdString(spec.id)},
            {"title", QString::fromStdString(spec.title)},
            {"left", layout.columns[c].bounds.left},
            {"right", layout.columns[c].bounds.right},
            {"headingBottom", layout.columns[c].heading.bottom},
            {"paper", QString::fromStdString(paperName(spec.paper))},
            {"landscape", spec.paper.width > spec.paper.height},
            {"background", QLatin1String(backgroundNames[std::size_t(spec.background)])},
            {"pages", int(std::ranges::count(spec.items, ColumnItemKind::Page, &ColumnItem::kind))},
        });
    }
    return list;
}

void BoardCanvas::showColumn(const QString &columnId) { showItem(columnId, {}); }

void BoardCanvas::syncPageOverlay() {
    if (!pageBoard_) {
        overlay_.sync({});
        return;
    }
    const Bounds visible =
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height()));
    const float margin = visible.height() / 4.0F;
    const PdfRenderer *pdfs = document_ ? document_->pdfRenderer() : nullptr;
    const auto missing = [pdfs](const std::string &blob) {
        return pdfs != nullptr && !pdfs->blobs().has(QString::fromStdString(blob));
    };
    overlay_.sync(pageOverlayRows(pageMap_, board().pages().columns(),
                                  {visible.left - margin, visible.top - margin,
                                   visible.right + margin, visible.bottom + margin},
                                  missing));
}

} // namespace peerchalk
