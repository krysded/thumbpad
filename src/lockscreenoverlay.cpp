#include "lockscreenoverlay.h"

#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>

namespace tp {

LockscreenOverlay::LockscreenOverlay()
    : QWaylandClientExtensionTemplate<LockscreenOverlay>(1)
{
    initialize();
}

void LockscreenOverlay::allowWindow(QWindow *window)
{
    window->create(); // the platform window must exist to get at its surface
    auto *ww = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!ww) {
        qWarning("thumbpad: no Wayland window for %s, not marked for the lock screen",
                 qPrintable(window->objectName()));
        return;
    }
    // KWin ignores "allow" for a surface that has no window yet, and rejects it
    // once the surface is mapped. The window exists once the surface has its
    // (layer-shell) role and isn't mapped until the first buffer, so mark it
    // right when Qt creates the role. Qt does that again each time a hidden
    // window is shown.
    connect(ww, &QNativeInterface::Private::QWaylandWindow::surfaceRoleCreated, this,
            [this, ww] {
                if (!isActive()) {
                    qWarning("thumbpad: lock screen overlay not bound, surface not marked");
                    return;
                }
                allow(ww->surface());
            },
            Qt::DirectConnection);
}

} // namespace tp
