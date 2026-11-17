#include "ui/app_style.hpp"

#include <QQuickStyle>

namespace peerchalk {

void useAppControlsStyle() { QQuickStyle::setStyle(QStringLiteral("Basic")); }

} // namespace peerchalk
