#pragma once

#include "qwayland-kde-lockscreen-overlay-v1.h"

#include <QtWaylandClient/QWaylandClientExtensionTemplate>

class QWindow;

namespace tp {

// kde_lockscreen_overlay_v1: lets marked surfaces stay visible while the
// screen is locked, so the keyboard can be used to type the password.
//
// KWin only offers this restricted protocol to programs whose .desktop file
// lists it in X-KDE-Wayland-Interfaces (see data/*.desktop.in).
class LockscreenOverlay : public QWaylandClientExtensionTemplate<LockscreenOverlay>,
                          public QtWayland::kde_lockscreen_overlay_v1
{
    Q_OBJECT
public:
    LockscreenOverlay();

    // Marks the window's surface every time Qt gives it its role, before it
    // is mapped, as the protocol requires. Call after the window is configured
    // (layer-shell) and before it is shown.
    void allowWindow(QWindow *window);
};

} // namespace tp
