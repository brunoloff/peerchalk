#include "library/instance_slot.hpp"

#include <emscripten/emscripten.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace peerchalk {
namespace {

// web_storage.js claimed the slot and loaded the databases before the app started.
QJsonObject storageState() {
    return QJsonDocument::fromJson(
               emscripten_run_script_string("JSON.stringify(peerchalkStorage.state())"))
        .object();
}

class BrowserSlot final : public InstanceSlot {
  public:
    BrowserSlot() {
        const QJsonObject state = storageState();
        index_ = state[QStringLiteral("index")].toInt();
        root_ = state[QStringLiteral("root")].toString();
        for (const QJsonValue &value : state[QStringLiteral("others")].toArray()) {
            const QJsonObject other = value.toObject();
            others_.push_back({other[QStringLiteral("index")].toInt(),
                               other[QStringLiteral("root")].toString(),
                               other[QStringLiteral("running")].toBool()});
        }
    }

    [[nodiscard]] int index() const override { return index_; }
    [[nodiscard]] QString root() const override { return root_; }
    [[nodiscard]] QString location() const override { return QStringLiteral("browser"); }
    [[nodiscard]] std::vector<OtherInstance> others() const override { return others_; }

    void finishMerge(const std::vector<OtherInstance> &merged) override {
        QJsonArray ended;
        for (const OtherInstance &other : merged)
            if (!other.running)
                ended.append(other.root);
        const QByteArray list = QJsonDocument(ended).toJson(QJsonDocument::Compact);
        emscripten_run_script(("peerchalkStorage.finishMerge(" + list + ")").constData());
    }

    [[nodiscard]] bool othersRunning() const override {
        return emscripten_run_script_int("peerchalkStorage.othersRunning() ? 1 : 0") != 0;
    }

  private:
    int index_{0};
    QString root_;
    std::vector<OtherInstance> others_;
};

} // namespace

std::unique_ptr<InstanceSlot> InstanceSlot::claim(const QString &) {
    return std::make_unique<BrowserSlot>();
}

} // namespace peerchalk
