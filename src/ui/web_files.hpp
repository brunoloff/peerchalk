#pragma once

#ifdef __EMSCRIPTEN__

#include <QByteArray>
#include <QString>

#include <functional>

namespace peerchalk {

// What the app needs from the web page: files, which have no real paths, and its address.
void downloadFile(const QString &name, const QByteArray &contents);
// Must run during a click. Calls chosen with the file's contents if one is picked.
void pickFile(std::function<void(QByteArray)> chosen);

// The page's address, the address the app is hosted at, and dropping the part after #.
[[nodiscard]] QString pageUrl();
[[nodiscard]] QString pageSiteUrl();
void removeSecretFromPageUrl();

} // namespace peerchalk

#endif
