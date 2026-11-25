// The board's own metadata, in the document's "meta" map.
#include "board/board_native.hpp"

namespace peerchalk {
namespace {

constexpr char metaName[] = "meta";
constexpr char nameKey[] = "name";

} // namespace

std::uint64_t Board::clientId() const { return ydoc_id(native_->doc.get()); }

std::string Board::name() const {
    Branch *meta = ymap(native_->doc.get(), metaName);
    if (meta == nullptr)
        throw std::runtime_error("Yrs board metadata could not be opened");
    const BoardTransaction txn = native_->read();
    return mapString(meta, txn.get(), nameKey);
}

void Board::setName(const std::string &name) {
    Branch *meta = ymap(native_->doc.get(), metaName);
    if (meta == nullptr)
        throw std::runtime_error("Yrs board metadata could not be opened");
    const BoardTransaction txn = native_->write(false);
    const YInput value = yinput_string(name.c_str());
    ymap_insert(meta, txn.get(), nameKey, &value);
}

} // namespace peerchalk
