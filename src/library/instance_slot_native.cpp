#include "library/instance_slot.hpp"

#include <QDir>
#include <QLockFile>
#include <QRegularExpression>

#include <map>
#include <set>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int maxInstances = 4096;

std::unique_ptr<QLockFile> lockFor(const QString &location, int index) {
    auto lock = std::make_unique<QLockFile>(
        QDir(location).filePath(QStringLiteral("instance-%1.lock").arg(index)));
    lock->setStaleLockTime(0); // only a lock whose process has ended is stale
    return lock;
}

QString rootFor(const QString &location, int index) {
    return index == 0 ? location
                      : QDir(location).filePath(QStringLiteral("instance-%1").arg(index));
}

class DesktopSlot final : public InstanceSlot {
  public:
    explicit DesktopSlot(QString location) : location_(std::move(location)) {
        if (!QDir().mkpath(location_))
            throw std::runtime_error("Could not create " + location_.toStdString());
        claimFirstFree();
        findOthers();
    }

    [[nodiscard]] int index() const override { return index_; }
    [[nodiscard]] QString root() const override { return rootFor(location_, index_); }
    [[nodiscard]] QString location() const override { return location_; }
    [[nodiscard]] std::vector<OtherInstance> others() const override { return others_; }

    void finishMerge(const std::vector<OtherInstance> &merged) override {
        for (const OtherInstance &other : merged)
            if (!other.running && other.index != 0)
                QDir(other.root).removeRecursively();
        ended_.clear(); // their slots are free again
    }

    [[nodiscard]] bool othersRunning() const override {
        const QStringList locks = QDir(location_).entryList({QStringLiteral("instance-*.lock")});
        for (const QString &name : locks) {
            const int index =
                name.section(QLatin1Char('-'), 1).section(QLatin1Char('.'), 0, 0).toInt();
            if (index == index_ || ended_.contains(index))
                continue;
            if (!lockFor(location_, index)->tryLock(0))
                return true;
        }
        return false;
    }

  private:
    void claimFirstFree() {
        for (int index = 0; index < maxInstances; ++index) {
            auto lock = lockFor(location_, index);
            if (lock->tryLock(0)) {
                index_ = index;
                lock_ = std::move(lock);
                if (!QDir().mkpath(root()))
                    throw std::runtime_error("Could not create " + root().toStdString());
                return;
            }
        }
        throw std::runtime_error("Too many PeerChalk instances are open");
    }

    void addOther(int index) {
        auto lock = lockFor(location_, index);
        const bool ended = lock->tryLock(0);
        others_.push_back({index, rootFor(location_, index), !ended});
        if (ended)
            ended_[index] = std::move(lock); // held so nobody claims it during the merge
    }

    void findOthers() {
        if (index_ != 0)
            addOther(0);
        static const QRegularExpression pattern(QStringLiteral("^instance-(\\d+)$"));
        for (const QString &name : QDir(location_).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QRegularExpressionMatch match = pattern.match(name);
            if (match.hasMatch() && match.captured(1).toInt() != index_)
                addOther(match.captured(1).toInt());
        }
    }

    QString location_;
    int index_{0};
    std::unique_ptr<QLockFile> lock_;
    std::vector<OtherInstance> others_;
    std::map<int, std::unique_ptr<QLockFile>> ended_;
};

} // namespace

std::unique_ptr<InstanceSlot> InstanceSlot::claim(const QString &location) {
    return std::make_unique<DesktopSlot>(location);
}

} // namespace peerchalk
