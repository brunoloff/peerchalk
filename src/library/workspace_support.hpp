#pragma once

// Helpers shared by the workspace's source files.

#include "library/access_log.hpp"

#include <QString>
#include <QUuid>

#include <stdexcept>

namespace peerchalk::workspace_detail {

inline constexpr int projectColorCount = 6;

[[nodiscard]] inline QString requireName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        throw std::invalid_argument("Names cannot be empty");
    return trimmed;
}

// A change of the given kind to a resource; the caller sets the action's fields.
[[nodiscard]] inline AccessOp change(ResourceKind kind, const QUuid &resource,
                                     AccessOp::Action action) {
    AccessOp op;
    op.kind = kind;
    op.resource = resource;
    op.action = action;
    return op;
}

} // namespace peerchalk::workspace_detail
