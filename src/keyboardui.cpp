#include "keyboardui.h"
#include "blockwindow.h"
#include "controller.h"

#include <LayerShellQt/window.h>
#include <QGuiApplication>
#include <QScreen>
#include <cmath>

namespace tp {

KeyboardUi::KeyboardUi(Controller *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    connect(controller, &Controller::stateChanged, this, &KeyboardUi::onStateChanged);
    connect(controller, &Controller::hideRequested, this, &KeyboardUi::hide);
    if (QScreen *s = QGuiApplication::primaryScreen())
        connect(s, &QScreen::geometryChanged, this, &KeyboardUi::applyBlocks);
}

KeyboardUi::~KeyboardUi()
{
    for (const auto &w : m_windows)
        delete w.data();
}

void KeyboardUi::setLayout(std::shared_ptr<const Layout> layout)
{
    for (const auto &w : m_windows)
        if (w)
            w->cancelAll();
    m_layout = std::move(layout);
    if (!m_layout->layers.contains(m_controller->layer()))
        m_controller->setLayer(QStringLiteral("main"));
    rebuild();
}

const LayerDef &KeyboardUi::currentLayer() const
{
    // A reference into m_layout (QMap::value() would return a copy): windows
    // keep pointers to these blocks until the next setLayout().
    auto it = m_layout->layers.constFind(m_controller->layer());
    if (it == m_layout->layers.cend())
        it = m_layout->layers.constFind(QStringLiteral("main"));
    return it.value();
}

bool KeyboardUi::portrait() const
{
    const QScreen *s = QGuiApplication::primaryScreen();
    return s && s->geometry().height() > s->geometry().width();
}

double KeyboardUi::pxPerMm() const
{
    const QScreen *s = QGuiApplication::primaryScreen();
    if (!s || s->physicalSize().isEmpty())
        return 96.0 / 25.4;
    // Diagonal ratio: independent of rotation.
    const QSizeF px = s->geometry().size();
    const QSizeF mm = s->physicalSize();
    return std::hypot(px.width(), px.height()) / std::hypot(mm.width(), mm.height());
}

void KeyboardUi::onStateChanged()
{
    if (m_controller->layer() != m_shownLayer)
        applyBlocks();
    for (const auto &w : m_windows)
        if (w)
            w->update();
}

void KeyboardUi::applyBlocks()
{
    if (!m_layout)
        return;
    const bool port = portrait();
    const QList<Block> &blocks = currentLayer().blocks(port);
    bool sameShape = port == m_shownPortrait && blocks.size() == m_windows.size();
    for (int i = 0; sameShape && i < blocks.size(); ++i)
        sameShape = m_windows[i] && m_windows[i]->block() &&
                    m_windows[i]->block()->edge == blocks[i].edge;
    if (!sameShape) {
        rebuild();
        return;
    }
    for (int i = 0; i < blocks.size(); ++i)
        m_windows[i]->setBlock(&blocks[i], pxPerMm());
    m_shownLayer = m_controller->layer();
}

void KeyboardUi::rebuild()
{
    // deleteLater: rebuild can run from inside one of these windows' touch handlers.
    for (const auto &w : m_windows) {
        if (w) {
            w->hide();
            w->deleteLater();
        }
    }
    m_windows.clear();
    if (!m_layout)
        return;

    const bool port = portrait();
    const Settings &st = m_layout->settings;
    const double ppm = pxPerMm();
    const bool wayland = QGuiApplication::platformName().startsWith("wayland");
    const QList<Block> &blocks = currentLayer().blocks(port);
    for (const Block &b : blocks) {
        auto *w = new BlockWindow(m_controller, st);
        if (wayland) {
            auto *lw = LayerShellQt::Window::get(w);
            lw->setLayer(LayerShellQt::Window::LayerOverlay);
            lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            lw->setScope(QStringLiteral("thumbpad"));
            LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorBottom;
            if (b.edge == "left")
                anchors |= LayerShellQt::Window::AnchorLeft;
            else if (b.edge == "right")
                anchors |= LayerShellQt::Window::AnchorRight;
            lw->setAnchors(anchors);
            const int edge = int(st.edgeMarginMm * ppm), bottom = int(st.bottomMarginMm * ppm);
            lw->setMargins(QMargins(edge, 0, edge, bottom));
            lw->setExclusiveZone(0);
        }
        w->setBlock(&b, ppm);
        m_windows << w;
    }
    m_shownLayer = m_controller->layer();
    m_shownPortrait = port;
    updateVisibility();
}

void KeyboardUi::updateVisibility()
{
    m_visible = m_pinned || (m_imActive && !m_suppressed);
    for (const auto &w : m_windows)
        if (w)
            w->setVisible(m_visible);
}

void KeyboardUi::show()
{
    m_pinned = true;
    m_suppressed = false;
    updateVisibility();
}

void KeyboardUi::hide()
{
    m_pinned = false;
    m_suppressed = true;
    updateVisibility();
}

void KeyboardUi::toggle()
{
    if (m_visible)
        hide();
    else
        show();
}

void KeyboardUi::setImActive(bool active)
{
    m_imActive = active;
    if (active)
        m_suppressed = false;
    updateVisibility();
}

} // namespace tp
