#include "repository/signed_content.hpp"
#include "repository/state_vector.hpp"
#include "repository_fixture.hpp"

#include <gtest/gtest.h>

namespace peerchalk::repository {
namespace {

using testing::Device;

TEST(StateVector, RoundTripsAndComparesClocks) {
    const StateVector vector{{1, 5}, {300, 2}, {0xffffffffffULL, 70000}};
    const std::optional<StateVector> decoded = decodeStateVector(encodeStateVector(vector));
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, vector);
    EXPECT_TRUE(covers(vector, {{1, 4}}));
    EXPECT_FALSE(covers(vector, {{1, 6}}));
    EXPECT_FALSE(covers(vector, {{2, 1}}));
    EXPECT_TRUE(covers({}, {{2, 0}}));
    EXPECT_EQ(merged({{1, 5}}, {{1, 7}, {2, 1}}), (StateVector{{1, 7}, {2, 1}}));
}

TEST(StateVector, RejectsMalformedBytes) {
    EXPECT_FALSE(decodeStateVector(QByteArray()));
    EXPECT_FALSE(decodeStateVector(QByteArray::fromHex("0201")));     // cut short
    EXPECT_FALSE(decodeStateVector(QByteArray::fromHex("01010100"))); // extra bytes
    EXPECT_FALSE(decodeStateVector(QByteArray(11, '\xff')));          // endless varint
}

class ContentTest : public ::testing::Test {
  protected:
    Device device_;
    ContentHeader header_{QUuid::createUuid(),   ContentKind::Changes, device_.author.certificate,
                          {QByteArray(32, 'h')}, {7, 1ULL << 40},      encodeStateVector({{7, 1}})};
};

TEST_F(ContentTest, RoundTripsHeaderAndBody) {
    const QByteArray file = sealContent(header_, QByteArray("body"), device_.author.device);
    const SignedContent content = openContent(file);
    EXPECT_EQ(content.body, QByteArray("body"));
    EXPECT_EQ(content.header.board, header_.board);
    EXPECT_EQ(content.header.certificate.userKey, device_.user());
    EXPECT_EQ(content.header.covers, header_.covers);
    EXPECT_EQ(content.header.clients, header_.clients);
}

TEST_F(ContentTest, RefusesAlteredForgedAndTruncatedFiles) {
    QByteArray file = sealContent(header_, QByteArray("body"), device_.author.device);
    QByteArray altered = file;
    altered[altered.size() - 66] = 'X'; // inside the body
    EXPECT_THROW((void)openContent(altered), FileError);
    EXPECT_THROW((void)openContent(file.first(file.size() - 1)), FileError);
    EXPECT_THROW((void)openContent(QByteArray("plain Yrs bytes")), FileError);
    const Device forger;
    const QByteArray forged = sealContent(header_, QByteArray("body"), forger.author.device);
    EXPECT_THROW((void)openContent(forged), FileError); // signed by another device
}

TEST_F(ContentTest, RefusesCertificateForAnotherDevice) {
    const Device other;
    header_.certificate.deviceKey = other.key(); // the certificate names device_'s key
    const QByteArray file = sealContent(header_, QByteArray("body"), other.author.device);
    EXPECT_THROW((void)openContent(file), FileError);
}

TEST(BoardFilesTest, BoardInfoRoundTripsAndRefusesOthers) {
    const BoardInfo info{QUuid::createUuid(), QByteArray(32, 'c')};
    const BoardInfo decoded = decodeBoardInfo(encodeBoardInfo(info));
    EXPECT_EQ(decoded.board, info.board);
    EXPECT_EQ(decoded.creator, info.creator);
    EXPECT_THROW((void)decodeBoardInfo("{}"), FileError);
    EXPECT_THROW((void)decodeBoardInfo("not json"), FileError);
    QByteArray newer = encodeBoardInfo(info);
    newer.replace("\"version\":1", "\"version\":2");
    EXPECT_THROW((void)decodeBoardInfo(newer), FileError);
}

TEST(BoardFilesTest, SecretsOpenOnlyForTheirUser) {
    const Device alice;
    const Device bob;
    const QByteArray capability(32, 's');
    const BoardSecret secret{QUuid::createUuid(), 2, alice.user(),
                             sealCapability(alice.user(), capability), bob.user()};
    const BoardSecret decoded = decodeSecretFile(encodeSecretFile(secret, bob.author.user));
    EXPECT_EQ(decoded.epoch, 2);
    EXPECT_EQ(alice.identity.openSealed(decoded.sealed, 64), capability);
    EXPECT_THROW((void)bob.identity.openSealed(decoded.sealed, 64), std::runtime_error);
    const QByteArray forged = encodeSecretFile(secret, alice.author.user); // signer is bob
    EXPECT_THROW((void)decodeSecretFile(forged), FileError);
}

TEST(BoardFilesTest, PersonRoundTripsAndRefusesForeignDevices) {
    const Device alice;
    const Device mallory;
    Person person{alice.user(),
                  QStringLiteral("Alice"),
                  QStringLiteral("a@example.org"),
                  {alice.author.certificate}};
    const Person decoded = decodePerson(encodePerson(person, alice.author.user));
    EXPECT_EQ(decoded.name, QStringLiteral("Alice"));
    ASSERT_EQ(decoded.devices.size(), 1U);
    EXPECT_EQ(decoded.devices.front().deviceKey, alice.key());
    person.devices.push_back(mallory.author.certificate); // certified by another user
    EXPECT_THROW((void)decodePerson(encodePerson(person, alice.author.user)), FileError);
    EXPECT_THROW((void)decodePerson(encodePerson(person, mallory.author.user)), FileError);
}

} // namespace
} // namespace peerchalk::repository
