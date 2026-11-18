#pragma once

#include <QString>

#include <memory>
#include <vector>

namespace peerchalk {

// Another running or finished copy of the app that uses the same data location.
struct OtherInstance {
    int index{0};
    QString root;
    bool running{false};
};

// Lets any number of app instances use one data location at once. Each claims its own
// numbered slot with its own files and device keys, so none overwrites another; the first
// slot is the location itself. At startup an instance merges the others' data, and slots
// left by instances that have ended are then removed. On the web the slots are browser
// databases guarded by Web Locks, on the desktop folders guarded by lock files.
class InstanceSlot {
  public:
    // Throws std::runtime_error when no slot can be claimed.
    [[nodiscard]] static std::unique_ptr<InstanceSlot> claim(const QString &location);
    virtual ~InstanceSlot() = default;

    [[nodiscard]] virtual int index() const = 0;
    [[nodiscard]] virtual QString root() const = 0;
    [[nodiscard]] virtual QString location() const = 0;
    // The other slots with data, as found when this one was claimed.
    [[nodiscard]] virtual std::vector<OtherInstance> others() const = 0;
    // Call once after merging: stops reading the others and removes the given ended ones.
    virtual void finishMerge(const std::vector<OtherInstance> &merged) = 0;
    // Whether another instance is running now.
    [[nodiscard]] virtual bool othersRunning() const = 0;
};

} // namespace peerchalk
