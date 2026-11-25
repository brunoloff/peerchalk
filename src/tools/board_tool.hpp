#pragma once

#include "library/user_identity.hpp"

#include <QJsonObject>
#include <QMap>
#include <QStringList>

#include <stdexcept>

namespace peerchalk::tools {

// peerchalk-board: the commands a program such as the VPvsVNP dashboard needs to keep
// repository boards (docs/board-files.md). Input that is not on the command line comes as a
// JSON object on standard input; every answer is a JSON object on standard output.

// A command line or input the tool cannot use: exit code 1.
class UsageError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
// Something the input asks for that the rules refuse: exit code 3.
class RefusedError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct Invocation {
    QString command;
    QStringList arguments;          // after the command, options removed
    QMap<QString, QString> options; // --name value
    QJsonObject input;              // standard input, for the commands that read it
};

// Runs one command with the given identity and returns its answer.
[[nodiscard]] QJsonObject runCommand(const Invocation &invocation, UserIdentity &identity);
// Whether the command reads standard input.
[[nodiscard]] bool readsInput(const QString &command);

} // namespace peerchalk::tools
