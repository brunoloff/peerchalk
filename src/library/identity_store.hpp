#pragma once

#include "library/codes.hpp"

#include <QByteArray>
#include <QString>

#include <vector>

namespace peerchalk {

// How a person's key was checked against what they see on their own screen.
enum class Verification { None, InPerson, Call, Shared };

struct Contact {
    QByteArray signingKey;
    QString name;
    Verification verification{Verification::None};
};

// This device's identity and the people it knows, kept in `directory`: identity.json
// holds the Ed25519 signing seed and X25519 recipient secret (owner-only, plain on disk
// for now), contacts.json the known people. The identity is created on first use.
// Read and write failures throw std::runtime_error naming the file; invalid arguments
// throw std::invalid_argument. Every change is saved before returning.
class IdentityStore {
  public:
    // defaultName is used when a new identity is created.
    IdentityStore(QString directory, const QString &defaultName);

    [[nodiscard]] QString displayName() const { return displayName_; }
    void setDisplayName(const QString &name);
    [[nodiscard]] int colorIndex() const { return colorIndex_; }
    void setColorIndex(int index);
    [[nodiscard]] QByteArray signingKey() const { return signingKey_; }
    [[nodiscard]] ContactCard contactCard() const { return {signingKey_, displayName_}; }

    [[nodiscard]] const std::vector<Contact> &contacts() const { return contacts_; }
    // Adds the person, or updates the name of someone already known. Throws
    // std::invalid_argument for your own card.
    void addContact(const ContactCard &card);
    void setVerification(const QByteArray &signingKey, Verification verification);
    void removeContact(const QByteArray &signingKey);

  private:
    [[nodiscard]] Contact &findContact(const QByteArray &signingKey);
    void loadIdentity(const QString &defaultName);
    void createIdentity(const QString &defaultName);
    void loadContacts();
    void saveIdentity(const QByteArray &signingSeed, const QByteArray &recipientSecret) const;
    void saveProfile() const;
    void saveContacts() const;

    QString directory_;
    QString displayName_;
    int colorIndex_{0};
    QByteArray signingKey_;
    std::vector<Contact> contacts_;
};

} // namespace peerchalk
