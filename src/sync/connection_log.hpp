#pragma once

#include <QDateTime>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <deque>

namespace peerchalk::sync {

// What happened while finding and connecting to peers, kept in memory for the troubleshooting
// page. Entries must hold no secrets: shortened device ids, names and server addresses only.
// Safe to use from any thread.
class ConnectionLog {
  public:
    enum class Level { Info, Warning, Error };
    struct Entry {
        QDateTime time;
        Level level;
        QString area; // such as "network", "discovery", "connection"
        QString text;
    };

    static constexpr std::size_t maxEntries = 5000;
    static constexpr qsizetype maxTextLength = 600;

    static ConnectionLog &instance();

    void add(Level level, const QString &area, const QString &text);
    // Oldest first, each as "2026-11-19 10:01:02.123Z WARN  area: text".
    [[nodiscard]] QStringList lines() const;
    // Counts additions, so viewers can tell whether to refresh.
    [[nodiscard]] quint64 revision() const;
    void clear();

    // Also records Qt warnings and errors, and PeerChalk's own messages, from anywhere in the
    // app. Messages still reach the previous handler.
    static void captureQtMessages();
    // Turns off Qt's desktop portal warning when PeerChalk runs as an AppImage, which installs
    // no desktop entry for the portal to find; nothing else depends on that registration.
    // QT_LOGGING_RULES still turns it back on.
    static void quietDesktopPortalInAppImages();

  private:
    mutable QMutex mutex_;
    std::deque<Entry> entries_;
    quint64 revision_{0};
};

void logInfo(const QString &area, const QString &text);
void logWarning(const QString &area, const QString &text);
void logError(const QString &area, const QString &text);

} // namespace peerchalk::sync
