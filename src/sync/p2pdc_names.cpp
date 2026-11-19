#include "sync/p2pdc_names.hpp"

#include <QByteArray>

namespace peerchalk::sync {
namespace {

using namespace p2pdc;

QString candidate(CandidateKind kind) {
    switch (kind) {
    case CandidateKind::Unknown: return QStringLiteral("unknown");
    case CandidateKind::Host: return QStringLiteral("host");
    case CandidateKind::ServerReflexive: return QStringLiteral("server-reflexive");
    case CandidateKind::PeerReflexive: return QStringLiteral("peer-reflexive");
    case CandidateKind::Relayed: return QStringLiteral("relayed");
    }
    return QStringLiteral("unknown");
}

} // namespace

QString describe(ConnectError error) {
    switch (error) {
    case ConnectError::NotReady: return QStringLiteral("not ready");
    case ConnectError::Unsupported: return QStringLiteral("unsupported");
    case ConnectError::InvalidPeer: return QStringLiteral("invalid peer");
    case ConnectError::ResourceLimit: return QStringLiteral("resource limit");
    case ConnectError::Backend: return QStringLiteral("backend failure");
    case ConnectError::Scheduler: return QStringLiteral("scheduler failure");
    case ConnectError::Timeout: return QStringLiteral("timed out");
    case ConnectError::Callback: return QStringLiteral("callback failure");
    case ConnectError::Signaling: return QStringLiteral("signaling failed");
    case ConnectError::Protocol: return QStringLiteral("protocol error");
    case ConnectError::Transport: return QStringLiteral("transport failed (no network path)");
    case ConnectError::Rejected: return QStringLiteral("rejected by the peer");
    case ConnectError::Canceled: return QStringLiteral("canceled");
    case ConnectError::Superseded: return QStringLiteral("superseded by another attempt");
    }
    return QStringLiteral("unknown error");
}

QString describe(ConnectionState state) {
    switch (state) {
    case ConnectionState::Connecting: return QStringLiteral("connecting");
    case ConnectionState::Ready: return QStringLiteral("ready");
    case ConnectionState::Suspended: return QStringLiteral("suspended");
    case ConnectionState::Failed: return QStringLiteral("failed");
    case ConnectionState::Closed: return QStringLiteral("closed");
    }
    return QStringLiteral("unknown state");
}

QString describe(WatchError error) {
    switch (error) {
    case WatchError::NotReady: return QStringLiteral("not ready");
    case WatchError::Unsupported: return QStringLiteral("unsupported");
    case WatchError::ResourceLimit: return QStringLiteral("resource limit");
    case WatchError::Backend: return QStringLiteral("backend failure");
    case WatchError::Crypto: return QStringLiteral("crypto failure");
    case WatchError::Clock: return QStringLiteral("clock problem");
    case WatchError::Scheduler: return QStringLiteral("scheduler failure");
    case WatchError::Callback: return QStringLiteral("callback failure");
    }
    return QStringLiteral("unknown error");
}

QString describe(AnnouncementError error) {
    switch (error) {
    case AnnouncementError::NotReady: return QStringLiteral("not ready");
    case AnnouncementError::Unsupported: return QStringLiteral("unsupported");
    case AnnouncementError::ResourceLimit: return QStringLiteral("resource limit");
    case AnnouncementError::PublicationLimit: return QStringLiteral("publication limit");
    case AnnouncementError::Retrying: return QStringLiteral("retrying");
    case AnnouncementError::Backend: return QStringLiteral("backend failure");
    case AnnouncementError::Crypto: return QStringLiteral("crypto failure");
    case AnnouncementError::Clock: return QStringLiteral("clock problem");
    case AnnouncementError::Scheduler: return QStringLiteral("scheduler failure");
    case AnnouncementError::Callback: return QStringLiteral("callback failure");
    }
    return QStringLiteral("unknown error");
}

QString describe(IncomingError error) {
    switch (error) {
    case IncomingError::NotReady: return QStringLiteral("not ready");
    case IncomingError::Unsupported: return QStringLiteral("unsupported");
    case IncomingError::InvalidContext: return QStringLiteral("invalid context");
    case IncomingError::ResourceLimit: return QStringLiteral("resource limit");
    case IncomingError::Backend: return QStringLiteral("backend failure");
    case IncomingError::Scheduler: return QStringLiteral("scheduler failure");
    case IncomingError::Timeout: return QStringLiteral("timed out");
    case IncomingError::Callback: return QStringLiteral("callback failure");
    case IncomingError::Canceled: return QStringLiteral("canceled");
    }
    return QStringLiteral("unknown error");
}

QString describe(NodeStartStatus status) {
    switch (status) {
    case NodeStartStatus::Started: return QStringLiteral("started");
    case NodeStartStatus::Canceled: return QStringLiteral("canceled");
    case NodeStartStatus::ResourceLimit: return QStringLiteral("resource limit");
    case NodeStartStatus::BackendFailure: return QStringLiteral("backend failure");
    }
    return QStringLiteral("unknown status");
}

QString describe(PeerEvent event) {
    switch (event) {
    case PeerEvent::Seen: return QStringLiteral("seen");
    case PeerEvent::Updated: return QStringLiteral("updated");
    case PeerEvent::Expired: return QStringLiteral("expired");
    }
    return QStringLiteral("unknown event");
}

QString describe(DiscoveryStatus status) {
    return status == DiscoveryStatus::Ready ? QStringLiteral("ready")
                                            : QStringLiteral("reconnecting");
}

QString describe(const RouteInfo &route) {
    QString kind = route.current == RouteKind::Direct        ? QStringLiteral("direct")
                   : route.current == RouteKind::TurnRelayed ? QStringLiteral("TURN relay")
                                                             : QStringLiteral("unknown route");
    if (route.selected_pair) {
        kind += QStringLiteral(" (%1 to %2)")
                    .arg(candidate(route.selected_pair->local),
                         candidate(route.selected_pair->remote));
    }
    if (route.turn_history == TurnHistory::Used)
        kind += QStringLiteral(", has used TURN");
    return kind;
}

QString describe(const std::exception_ptr &error) {
    if (!error)
        return {};
    try {
        std::rethrow_exception(error);
    } catch (const std::exception &exception) {
        return QString::fromUtf8(exception.what());
    } catch (...) {
        return QStringLiteral("unknown exception");
    }
}

QString shortDevice(const SigningPublicKey &key) {
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(key.data()), 4).toHex());
}

} // namespace peerchalk::sync
