// Library: board links.
#include "library/codes.hpp"
#include "ui/library.hpp"

#include <QUuid>

#include <QtLogging>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {
namespace {

QUuid boardId(const QString &text) {
    const QUuid id = QUuid::fromString(text);
    if (id.isNull())
        throw std::invalid_argument("Unknown board");
    return id;
}

} // namespace

QString Library::boardLink(const QString &id, const QString &siteUrl) {
    QString text;
    attempt([&] {
        if (const auto link = workspace_.currentBoardLink(boardId(id)))
            text = encodeBoardLink(*link, siteUrl);
    });
    return text;
}

QString Library::createBoardLink(const QString &id, const QString &siteUrl) {
    QString text;
    if (attempt([&] { text = encodeBoardLink(workspace_.createBoardLink(boardId(id)), siteUrl); }))
        workspaceUpdated();
    return text;
}

void Library::disableBoardLink(const QString &id) {
    if (attempt([&] { workspace_.disableBoardLink(boardId(id)); }))
        workspaceUpdated();
}

QString Library::openBoardLink(const QString &url) {
    QString id;
    attempt([&] {
        if (const std::optional<BoardLink> link = decodeBoardLink(url)) {
            workspace_.joinByLink(*link);
            id = link->id.toString(QUuid::WithoutBraces);
            return;
        }
        // After a link is used its secret leaves the address bar; the board is known by then.
        const std::optional<QUuid> known = boardIdFromLink(url);
        if (!known || !workspace_.hasBoard(*known))
            throw std::invalid_argument("This board link is incomplete. Ask for it again.");
        id = known->toString(QUuid::WithoutBraces);
    });
    if (!id.isEmpty()) {
        const QUuid board = QUuid::fromString(id);
        const bool inProject = std::ranges::any_of(workspace_.projects(), [&](const auto &project) {
            const std::vector<QUuid> boards = workspace_.boardsInProject(project.id);
            return std::ranges::find(boards, board) != boards.end();
        });
        if (!inProject)
            currentProject_ = sharedId();
        workspaceUpdated();
    }
    return id;
}

bool Library::admitByLink(const QUuid &board, const QByteArray &user, const QString &name,
                          const QByteArray &token) {
    bool admitted = false;
    try {
        admitted = workspace_.admitByLink(board, user, name, token);
    } catch (const std::invalid_argument &error) {
        qWarning("PeerChalk did not admit someone by link: %s", error.what());
    }
    if (admitted)
        workspaceUpdated();
    return admitted;
}

} // namespace peerchalk
