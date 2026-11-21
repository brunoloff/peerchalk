#include "ui/web_files.hpp"

#ifdef __EMSCRIPTEN__

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <utility>

namespace {

QByteArray pickedFile;
QByteArray pickedName;
std::function<void(QByteArray, QString)> pickedCallback;

EM_JS(void, peerchalk_download,
      (const char *name, unsigned nameSize, const char *data, unsigned size),
      { peerchalkDownloadFile(name, nameSize, data, size); });

EM_JS(void, peerchalk_pick_file, (const char *accept),
      { peerchalkPickFile(UTF8ToString(accept)); });

EM_JS_DEPS(peerchalk_files, "$UTF8ToString");

} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE char *peerchalk_file_buffer(unsigned size) {
    pickedFile.resize(static_cast<qsizetype>(size));
    return pickedFile.data();
}

EMSCRIPTEN_KEEPALIVE char *peerchalk_file_name_buffer(unsigned size) {
    pickedName.resize(static_cast<qsizetype>(size));
    return pickedName.data();
}

EMSCRIPTEN_KEEPALIVE void peerchalk_file_ready() {
    if (pickedCallback)
        std::exchange(pickedCallback, {})(std::exchange(pickedFile, {}),
                                          QString::fromUtf8(std::exchange(pickedName, {})));
}
}

namespace peerchalk {

void downloadFile(const QString &name, const QByteArray &contents) {
    const QByteArray encodedName = name.toUtf8();
    peerchalk_download(encodedName.constData(), static_cast<unsigned>(encodedName.size()),
                       contents.constData(), static_cast<unsigned>(contents.size()));
}

QString pageUrl() { return QString::fromUtf8(emscripten_run_script_string("location.href")); }

QString pageSiteUrl() {
    return QString::fromUtf8(emscripten_run_script_string("new URL('.', document.baseURI).href"));
}

void removeSecretFromPageUrl() {
    emscripten_run_script("history.replaceState(null, '', location.pathname)");
}

QString pageUserAgent() {
    return QString::fromUtf8(emscripten_run_script_string("navigator.userAgent"));
}

void pickFile(const QString &accept, std::function<void(QByteArray, QString)> chosen) {
    pickedCallback = std::move(chosen);
    peerchalk_pick_file(accept.toUtf8().constData());
}

} // namespace peerchalk

#endif
