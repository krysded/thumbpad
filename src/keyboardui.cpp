#include "keyboardui.h"
#include "blockwindow.h"
#include "controller.h"

#include <LayerShellQt/window.h>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <cmath>

namespace tp {

namespace {

bool onWayland()
{
    return QGuiApplication::platformName().startsWith("wayland");
}

QSettings stateSettings()
{
    // ~/.config/thumbpad/state.ini: runtime state, not part of the layout.
    return QSettings(QSettings::IniFormat, QSettings::UserScope, "thumbpad", "state");
}

} // namespace

SpacerWindow::SpacerWindow()
{
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    resize(1, 1);
}

void SpacerWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(QPoint(), size()), Qt::transparent);
}

KeyboardUi::KeyboardUi(Controller *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    connect(controller, &Controller::stateChanged, this, &KeyboardUi::onStateChanged);
    connect(controller, &Controller::hideRequested, this, &KeyboardUi::hide);
    connect(controller, &Controller::commandRequested, this, &KeyboardUi::onCommand);
    if (QScreen *s = QGuiApplication::primaryScreen())
        connect(s, &QScreen::geometryChanged, this, &KeyboardUi::applyBlocks);

    QSettings st = stateSettings();
    m_reserve = st.value("reserveSpace", false).toBool();
    st.beginGroup("offsets");
    for (const QString &k : st.childKeys())
        m_offsets.insert(QString(k).replace('_', '/'), st.value(k).toPointF());
}

KeyboardUi::~KeyboardUi()
{
    for (const auto &w : m_windows)
        delete w.data();
    delete m_spacer.data();
}

void KeyboardUi::saveState() const
{
    QSettings st = stateSettings();
    st.setValue("reserveSpace", m_reserve);
    st.remove("offsets");
    st.beginGroup("offsets");
    for (auto it = m_offsets.cbegin(); it != m_offsets.cend(); ++it)
        st.setValue(QString(it.key()).replace('/', '_'), it.value());
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

QString KeyboardUi::offsetKey(int index) const
{
    return QStringLiteral("%1/%2").arg(m_shownPortrait ? "portrait" : "landscape").arg(index);
}

void KeyboardUi::onStateChanged()
{
    if (m_controller->layer() != m_shownLayer)
        applyBlocks();
    for (const auto &w : m_windows)
        if (w)
            w->update();
}

void KeyboardUi::onCommand(const QString &command)
{
    if (command == "toggle-reserve")
        setReserveSpace(!m_reserve);
    else if (command == "reset-position")
        resetPositions();
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
    for (int i = 0; i < blocks.size(); ++i) {
        m_windows[i]->setBlock(&blocks[i], pxPerMm());
        place(i); // the size may differ between layers
    }
    m_shownLayer = m_controller->layer();
    updateSpacer();
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
    m_shownPortrait = port;
    m_shownLayer = m_controller->layer();
    const Settings &st = m_layout->settings;
    const double ppm = pxPerMm();
    const QList<Block> &blocks = currentLayer().blocks(port);
    for (const Block &b : blocks) {
        auto *w = new BlockWindow(m_controller, st);
        if (onWayland()) {
            auto *lw = LayerShellQt::Window::get(w);
            lw->setLayer(LayerShellQt::Window::LayerOverlay);
            lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            lw->setScope(QStringLiteral("thumbpad"));
            // -1: ignore other surfaces' exclusive zones, including our own spacer.
            lw->setExclusiveZone(-1);
        }
        w->setBlock(&b, ppm);
        connect(w, &BlockWindow::moveRequested, this, [this](BlockWindow *self, QPointF d) {
            moveBlock(int(m_windows.indexOf(self)), d);
        });
        connect(w, &BlockWindow::moveFinished, this, &KeyboardUi::saveState);
        m_windows << w;
        place(int(m_windows.size()) - 1);
    }

    if (onWayland() && !m_spacer) {
        m_spacer = new SpacerWindow;
        auto *lw = LayerShellQt::Window::get(m_spacer);
        lw->setLayer(LayerShellQt::Window::LayerBottom);
        lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        lw->setScope(QStringLiteral("thumbpad-spacer"));
        lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom |
                                                     LayerShellQt::Window::AnchorLeft |
                                                     LayerShellQt::Window::AnchorRight));
    }
    updateSpacer();
    updateVisibility();
}

void KeyboardUi::place(int index)
{
    BlockWindow *w = m_windows.value(index);
    if (!w || !w->block() || !m_layout)
        return;
    const QScreen *s = QGuiApplication::primaryScreen();
    const QSize screen = s ? s->geometry().size() : QSize(1368, 912);
    const double ppm = pxPerMm();
    const Settings &st = m_layout->settings;
    const QString &edge = w->block()->edge;
    const double maxSide = std::max(0, screen.width() - w->width());
    const double maxBottom = std::max(0, screen.height() - w->height());

    // Offsets are in screen axes (x right, y down) relative to the default spot.
    QPointF &off = m_offsets[offsetKey(index)];
    const double baseSide = edge == "center" ? (screen.width() - w->width()) / 2.0
                                             : st.edgeMarginMm * ppm;
    const double baseBottom = st.bottomMarginMm * ppm;
    const double sign = edge == "right" ? -1 : 1; // right-anchored: x grows inwards
    const double side = std::clamp(baseSide + sign * off.x(), 0.0, maxSide);
    const double bottom = std::clamp(baseBottom - off.y(), 0.0, maxBottom);
    off = QPointF(sign * (side - baseSide), baseBottom - bottom); // store the clamped value

    if (!onWayland())
        return;
    auto *lw = LayerShellQt::Window::get(w);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorBottom;
    anchors |= edge == "right" ? LayerShellQt::Window::AnchorRight
                               : LayerShellQt::Window::AnchorLeft;
    lw->setAnchors(anchors);
    lw->setMargins(edge == "right" ? QMargins(0, 0, int(side), int(bottom))
                                   : QMargins(int(side), 0, 0, int(bottom)));
}

void KeyboardUi::moveBlock(int index, QPointF delta)
{
    if (index < 0 || index >= m_windows.size())
        return;
    m_offsets[offsetKey(index)] += delta;
    place(index);
    updateSpacer();
}

void KeyboardUi::resetPositions()
{
    m_offsets.clear();
    for (int i = 0; i < m_windows.size(); ++i)
        place(i);
    updateSpacer();
    saveState();
}

void KeyboardUi::setReserveSpace(bool reserve)
{
    m_reserve = reserve;
    updateSpacer();
    updateVisibility();
    saveState();
}

void KeyboardUi::updateSpacer()
{
    if (!m_spacer)
        return;
    // Reserve up to the top of the highest block.
    int zone = 0;
    for (int i = 0; i < m_windows.size(); ++i) {
        const BlockWindow *w = m_windows[i];
        if (!w)
            continue;
        const auto *lw = LayerShellQt::Window::get(const_cast<BlockWindow *>(w));
        zone = std::max(zone, lw->margins().bottom() + w->height());
    }
    LayerShellQt::Window::get(m_spacer)->setExclusiveZone(zone);
}

void KeyboardUi::updateVisibility()
{
    m_visible = m_pinned || (m_imActive && !m_suppressed);
    for (const auto &w : m_windows)
        if (w)
            w->setVisible(m_visible);
    if (m_spacer)
        m_spacer->setVisible(m_visible && m_reserve);
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
