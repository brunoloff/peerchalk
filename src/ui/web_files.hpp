#pragma once

#ifdef __EMSCRIPTEN__

#include <QByteArray>
#include <QString>

#include <functional>

namespace peerchalk {

// What the app needs from the web page: files, which have no real paths, and its address.
void downloadFile(const QString &name, const QByteArray &contents);
// Must run during a click. accept lists the file types offered (".peerchalk", ".pdf"). Calls
// chosen with the file's contents and name if one is picked.
void pickFile(const QString &accept, std::function<void(QByteArray, QString)> chosen);

// The page's address, the address the app is hosted at, and dropping the part after #.
[[nodiscard]] QString pageUrl();
[[nodiscard]] QString pageSiteUrl();
void removeSecretFromPageUrl();
// The browser's user agent string.
[[nodiscard]] QString pageUserAgent();

} // namespace peerchalk

#endif
