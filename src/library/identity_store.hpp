#pragma once

#include "library/codes.hpp"
#include "library/user_identity.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace peerchalk {

// How a person's key was checked against what they see on their own screen.
enum class Verification { None, InPerson, Call, Shared };

struct Contact {
    QByteArray signingKey; // their user key
    QString name;
    Verification verification{Verification::None};
};

// This installation's identity (see UserIdentity), your profile and the people you know,
// kept in `directory`: profile.json and contacts.json beside the key files. Read and
// write failures throw std::runtime_error naming the file; invalid arguments throw
// std::invalid_argument. Every change is saved before returning.
class IdentityStore {
  public:
    // defaultName is used when there is no profile yet.
    IdentityStore(QString directory, const QString &defaultName);

    [[nodiscard]] const UserIdentity &identity() const { return identity_; }
    [[nodiscard]] QString displayName() const { return displayName_; }
    void setDisplayName(const QString &name);
    [[nodiscard]] int colorIndex() const { return colorIndex_; }
    void setColorIndex(int index);
    [[nodiscard]] QByteArray signingKey() const { return identity_.userKey(); }
    [[nodiscard]] ContactCard contactCard() const { return {signingKey(), displayName_}; }
    // Becomes the user in a transfer code (see UserIdentity::adopt) and takes its name.
    void adoptIdentity(const QString &transferCode);

    [[nodiscard]] const std::vector<Contact> &contacts() const { return contacts_; }
    // Adds the person, or updates the name of someone already known. Throws
    // std::invalid_argument for your own card.
    void addContact(const ContactCard &card);
    void setVerification(const QByteArray &signingKey, Verification verification);
    void removeContact(const QByteArray &signingKey);
    // Contacts for your other devices; merging adds unknown people and verifications
    // made elsewhere, and returns true when something changed. Throws
    // std::runtime_error for malformed input.
    [[nodiscard]] QJsonObject exportContacts() const;
    bool mergeContacts(const QJsonObject &exported);

  private:
    [[nodiscard]] Contact &findContact(const QByteArray &signingKey);
    void loadProfile(const QString &defaultName);
    void loadContacts();
    void saveProfile() const;
    void saveContacts() const;

    QString directory_;
    UserIdentity identity_;
    QString displayName_;
    int colorIndex_{0};
    std::vector<Contact> contacts_;
};

} // namespace peerchalk
