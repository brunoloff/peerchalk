#pragma once

#include "library/signatures.hpp"

#include <QByteArray>
#include <QUuid>

#include <cstdint>
#include <vector>

namespace peerchalk::repository {

// A board's content in the repository: a snapshot (snapshots/<sha256>.yrs) or a device's
// change file (changes/<device>.yrs). The envelope is
//   "PCRB" 0x01, u32 header length, header (compact JSON), u32 body length, body (a Yrs v1
//   update), and 64 bytes: the device key's signature over everything before.
// The header carries the device's certificate, so a file can be checked on its own.
enum class ContentKind { Snapshot, Changes };

struct ContentHeader {
    QUuid board;
    ContentKind kind{ContentKind::Changes};
    DeviceCertificate certificate;      // names the device and the user it acts for
    std::vector<QByteArray> covers;     // SHA-256 of the snapshots this file builds on
    std::vector<std::uint64_t> clients; // Yrs client ids the device has written with
    // Changes: the Yrs state vector of everything the device knew when writing, which the
    // body (relative to the snapshots in covers) may not show on its own. The device decides
    // with it when its file is covered; nobody else relies on it.
    QByteArray stateVector;
};

struct SignedContent {
    ContentHeader header;
    QByteArray body;
};

// Seals content with the device key of header.certificate; throws std::invalid_argument
// for a header without a board or certificate.
[[nodiscard]] QByteArray sealContent(const ContentHeader &header, const QByteArray &body,
                                     const Signer &deviceSigner);
// Checks the envelope, the certificate and the signature; throws FileError naming what is
// wrong. Whether the certificate's user may write content is the caller's question.
[[nodiscard]] SignedContent openContent(const QByteArray &file);

} // namespace peerchalk::repository
