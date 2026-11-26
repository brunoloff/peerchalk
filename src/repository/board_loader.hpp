#pragma once

#include "library/access_log.hpp"
#include "repository/board_files.hpp"
#include "repository/state_vector.hpp"

#include <QByteArray>
#include <QString>

#include <map>
#include <vector>

namespace peerchalk::repository {

// One board's files as stored on the branch, by path relative to boards/<id>/.
using BoardFiles = std::map<QString, QByteArray>;

// A file the loader skipped, and why.
struct LoadWarning {
    QString path;
    QString reason;
};

// A content file that was accepted.
struct AcceptedContent {
    QString path;
    QByteArray hash;   // SHA-256 of the file
    QByteArray device; // device key
    QByteArray user;   // the user the device acts for
    StateVector state; // snapshots: what the body holds; changes: what the device knew
    std::vector<QByteArray> covers;
    std::vector<std::uint64_t> clients;
    QByteArray body; // the Yrs update it holds
};

// The result of loading a repository board.
struct LoadedBoard {
    BoardInfo info;
    std::vector<SignedOp> ops; // every valid access entry known, files and extra ones
    AccessState access;
    QByteArray state; // the merged Yrs document of every accepted content file
    std::vector<AcceptedContent> snapshots;
    std::vector<AcceptedContent> changes;
    std::map<int, std::vector<BoardSecret>> secrets; // accepted ones, by epoch
    std::vector<LoadWarning> warnings;
    QString name; // from the document, else from the access log

    // What the accepted snapshots hold together.
    [[nodiscard]] StateVector snapshotState() const;
    // The newest epoch, or -1 when there is no accepted secret.
    [[nodiscard]] int latestEpoch() const;
};

// Loads a board: board.json, the access log (the board's access files plus extraAccess,
// entries from the branch's history and ones seen before), and every snapshot and change
// file signed by a device of a member or above. Anything else is skipped with a warning.
// Throws FileError when board.json is missing or malformed.
[[nodiscard]] LoadedBoard loadBoard(const BoardFiles &files,
                                    const std::vector<QByteArray> &extraAccess);

} // namespace peerchalk::repository
