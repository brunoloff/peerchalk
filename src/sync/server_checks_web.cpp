// Server checks in browsers, through web_checks.js: browsers cannot open TLS or UDP sockets,
// so web servers get a WebSocket or HTTP request and STUN and TURN servers a WebRTC
// candidate search.
#include "sync/server_checks.hpp"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <map>

namespace {

peerchalk::sync::ServerChecks *activeChecks = nullptr;

EM_JS(void, peerchalk_check_server,
      (int id, const char *kind, const char *address, const char *login), {
          peerchalkCheckServer(id, UTF8ToString(kind), UTF8ToString(address), UTF8ToString(login));
      });

} // namespace

namespace peerchalk::sync {

struct ServerChecks::Impl {
    std::map<int, QString> running; // check id → target key
    int nextId{1};
};

ServerChecks::ServerChecks(QObject *parent) : QObject(parent), impl_(std::make_unique<Impl>()) {
    activeChecks = this;
}

ServerChecks::~ServerChecks() {
    if (activeChecks == this)
        activeChecks = nullptr;
}

void ServerChecks::check(const std::vector<ServerTarget> &targets) {
    impl_->running.clear(); // answers to earlier checks are ignored
    for (const ServerTarget &target : targets) {
        const int id = impl_->nextId++;
        impl_->running[id] = target.key;
        peerchalk_check_server(id, serverKindName(target.kind), target.address.toUtf8().constData(),
                               target.login.toUtf8().constData());
    }
}

void ServerChecks::answered(int id, bool working, int milliseconds) {
    const auto found = impl_->running.find(id);
    if (found == impl_->running.end())
        return;
    const QString key = found->second;
    impl_->running.erase(found);
    emit checked(key, working, milliseconds);
}

} // namespace peerchalk::sync

extern "C" EMSCRIPTEN_KEEPALIVE void peerchalk_server_checked(int id, int working,
                                                              int milliseconds) {
    if (activeChecks != nullptr)
        activeChecks->answered(id, working != 0, milliseconds);
}
