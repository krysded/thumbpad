#include "settingswindow.h"
#include "keyboardui.h"
#include "usersettings.h"

#include <LayerShellQt/window.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>

namespace tp {

SettingsWindow::SettingsWindow(UserSettings *settings, KeyboardUi *ui, QObject *parent)
    : QObject(parent), m_settings(settings), m_ui(ui)
{
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::open()
{
    if (!m_engine) {
        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->rootContext()->setContextProperty("settings", m_settings);
        m_engine->rootContext()->setContextProperty("keyboard", m_ui);
        m_engine->load(QUrl(QStringLiteral("qrc:/qml/Settings.qml")));
        if (m_engine->rootObjects().isEmpty()) {
            qWarning("thumbpad: cannot load the settings window");
            m_engine.reset();
            return;
        }
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        if (QGuiApplication::platformName().startsWith("wayland")) {
            // A layer-shell panel takes no keyboard focus, so the text field
            // stays focused and the keyboard stays up while you tune it.
            auto *lw = LayerShellQt::Window::get(m_window);
            lw->setLayer(LayerShellQt::Window::LayerOverlay);
            lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            lw->setScope(QStringLiteral("thumbpad-settings"));
            lw->setExclusiveZone(-1);
            lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop |
                                                         LayerShellQt::Window::AnchorBottom |
                                                         LayerShellQt::Window::AnchorLeft));
        }
        // Keep the panel clear of the keyboard as it changes size or moves.
        connect(m_ui, &KeyboardUi::geometryChanged, this, &SettingsWindow::place);
        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            m_ui->setSettingsOpen(visible);
        });
    }
    place();
    m_window->show();
    m_window->raise();
}

void SettingsWindow::place()
{
    if (!m_window || !QGuiApplication::platformName().startsWith("wayland"))
        return;
    const QScreen *s = QGuiApplication::primaryScreen();
    const int screenH = s ? s->geometry().height() : 912;
    const QRect free = m_ui->freeArea(400);
    const int w = std::min(640, free.width() - 32);
    m_window->setWidth(w);
    // Anchored top, bottom and left: the height follows from the margins.
    LayerShellQt::Window::get(m_window)->setMargins(
        QMargins(free.x() + (free.width() - w) / 2, free.y() + 16, 0,
                 screenH - free.bottom() - 1 + 16));
}

} // namespace tp
