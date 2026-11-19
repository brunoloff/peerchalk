#include "sync/connection_log.hpp"

#include <QLoggingCategory>
#include <QMutexLocker>

namespace peerchalk::sync {
namespace {

QtMessageHandler previousHandler = nullptr;
thread_local bool insideHandler = false;

QString levelName(ConnectionLog::Level level) {
    switch (level) {
    case ConnectionLog::Level::Info: return QStringLiteral("INFO ");
    case ConnectionLog::Level::Warning: return QStringLiteral("WARN ");
    case ConnectionLog::Level::Error: return QStringLiteral("ERROR");
    }
    return QStringLiteral("?    ");
}

bool isOwnMessage(const QString &text) {
    return text.startsWith(QLatin1String("PeerChalk")) || text.startsWith(QLatin1String("p2pdc"));
}

void recordQtMessage(QtMsgType type, const QMessageLogContext &context, const QString &text) {
    const bool problem = type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg;
    if (!problem && !isOwnMessage(text))
        return;
    const QString area = context.category != nullptr && qstrcmp(context.category, "default") != 0
                             ? QString::fromLatin1(context.category)
                             : QStringLiteral("app");
    const auto level =
        type == QtWarningMsg ? ConnectionLog::Level::Warning
        : problem            ? ConnectionLog::Level::Error
                             : ConnectionLog::Level::Info;
    ConnectionLog::instance().add(level, area, text);
}

void handleQtMessage(QtMsgType type, const QMessageLogContext &context, const QString &text) {
    if (!insideHandler) {
        insideHandler = true;
        recordQtMessage(type, context, text);
        insideHandler = false;
    }
    if (previousHandler != nullptr)
        previousHandler(type, context, text);
}

} // namespace

ConnectionLog &ConnectionLog::instance() {
    static ConnectionLog log;
    return log;
}

void ConnectionLog::add(Level level, const QString &area, const QString &text) {
    Entry entry{QDateTime::currentDateTimeUtc(), level, area,
                text.size() > maxTextLength ? text.left(maxTextLength) + QStringLiteral("…")
                                            : text};
    const QMutexLocker lock(&mutex_);
    entries_.push_back(std::move(entry));
    if (entries_.size() > maxEntries)
        entries_.pop_front();
    ++revision_;
}

QStringList ConnectionLog::lines() const {
    const QMutexLocker lock(&mutex_);
    QStringList lines;
    lines.reserve(qsizetype(entries_.size()));
    for (const Entry &entry : entries_) {
        lines.push_back(QStringLiteral("%1 %2 %3: %4")
                            .arg(entry.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz'Z'")),
                                 levelName(entry.level), entry.area, entry.text));
    }
    return lines;
}

quint64 ConnectionLog::revision() const {
    const QMutexLocker lock(&mutex_);
    return revision_;
}

void ConnectionLog::clear() {
    const QMutexLocker lock(&mutex_);
    entries_.clear();
    ++revision_;
}

void ConnectionLog::captureQtMessages() {
    const QtMessageHandler previous = qInstallMessageHandler(handleQtMessage);
    if (previous != handleQtMessage)
        previousHandler = previous;
}

void ConnectionLog::quietDesktopPortalInAppImages() {
    if (qEnvironmentVariableIsSet("APPIMAGE"))
        QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.services.warning=false"));
}

void logInfo(const QString &area, const QString &text) {
    ConnectionLog::instance().add(ConnectionLog::Level::Info, area, text);
}

void logWarning(const QString &area, const QString &text) {
    ConnectionLog::instance().add(ConnectionLog::Level::Warning, area, text);
}

void logError(const QString &area, const QString &text) {
    ConnectionLog::instance().add(ConnectionLog::Level::Error, area, text);
}

} // namespace peerchalk::sync
