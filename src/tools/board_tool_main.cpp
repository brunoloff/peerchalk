// peerchalk-board [--user-dir U] [--device-dir D] <command> [arguments] [--option value]
//
//   identity                          this device's keys and certificate
//   identity-import <transfer code>   become the user of a PeerChalk "Link a device" code
//   device-secrets [--name N]         the secrets a hosted PeerChalk needs (keep them private)
//   person --name N [--email E]       this person's people/<user>.json
//   create --name N [--display-name D] a new board's files
//   list     (stdin {"boards": {id: board}})  each board's name, people and warnings
//   open     (stdin board)                    one board, with its network secret
//   set-role (stdin board) --user K --role member|owner [--name N]
//
// A board on standard input is {"files": {path under boards/<id>/: base64},
// "access": [base64 access entries from the branch's history]}. Answers are one JSON object
// on standard output. Exit codes: 0 done, 1 bad command line or input, 2 unreadable files,
// 3 refused by the board's rules.
#include "tools/board_tool.hpp"

#include <QFile>
#include <QJsonDocument>

#include <cstdio>
#include <iostream>

namespace {

using peerchalk::tools::Invocation;
using peerchalk::tools::RefusedError;
using peerchalk::tools::UsageError;

struct Folders {
    QString user;
    QString device;
};

Invocation parse(const QStringList &args, Folders &folders) {
    Invocation call;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (!arg.startsWith(QStringLiteral("--"))) {
            if (call.command.isEmpty())
                call.command = arg;
            else
                call.arguments << arg;
            continue;
        }
        if (i + 1 >= args.size())
            throw UsageError(arg.toStdString() + " needs a value");
        const QString value = args[++i];
        if (arg == QStringLiteral("--user-dir"))
            folders.user = value;
        else if (arg == QStringLiteral("--device-dir"))
            folders.device = value;
        else
            call.options[arg.mid(2)] = value;
    }
    if (call.command.isEmpty() || folders.user.isEmpty())
        throw UsageError("usage: peerchalk-board --user-dir U [--device-dir D] <command> ...");
    if (folders.device.isEmpty())
        folders.device = folders.user;
    return call;
}

QJsonObject readInput() {
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly))
        throw UsageError("standard input could not be read");
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw UsageError("standard input is not a JSON object");
    return document.object();
}

int answer(const QJsonObject &object, int code) {
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return code;
}

} // namespace

int main(int argc, char **argv) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    try {
        Folders folders;
        Invocation call = parse(args, folders);
        if (peerchalk::tools::readsInput(call.command))
            call.input = readInput();
        peerchalk::UserIdentity identity(folders.user, folders.device);
        return answer(peerchalk::tools::runCommand(call, identity), 0);
    } catch (const UsageError &error) {
        return answer({{"error", error.what()}}, 1);
    } catch (const RefusedError &error) {
        return answer({{"error", error.what()}}, 3);
    } catch (const std::exception &error) {
        return answer({{"error", error.what()}}, 2);
    }
}
