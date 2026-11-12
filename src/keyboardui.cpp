#include "keyboardui.h"
#include "blockwindow.h"
#include "controller.h"

#include <LayerShellQt/window.h>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
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

HandleWindow::HandleWindow()
{
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
}

bool HandleWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        e->accept();
        return true;
    case QEvent::TouchEnd:
        e->accept();
        Q_EMIT tapped();
        return true;
    case QEvent::MouseButtonRelease:
        Q_EMIT tapped();
        return true;
    default:
        return QRasterWindow::event(e);
    }
}

void HandleWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(QPoint(), size()), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // A tab sticking out of the right edge: rounded on the left side only.
    const QRectF r(0, 0, width() + 12, height());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(43, 46, 54, 200));
    p.drawRoundedRect(r, 10, 10);
    QFont f = p.font();
    f.setPixelSize(int(width() * 0.6));
    p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, QStringLiteral("⌨"));
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
    for (const QString &k : {QStringLiteral("landscape"), QStringLiteral("portrait")})
        if (st.contains(k))
            m_offsets.insert(k, st.value(k).toPointF());
}

KeyboardUi::~KeyboardUi()
{
    for (const auto &w : m_windows)
        delete w.data();
    delete m_spacer.data();
    delete m_handle.data();
}

void KeyboardUi::saveState() const
{
    QSettings st = stateSettings();
    st.setValue("reserveSpace", m_reserve);
    st.remove("offsets");
    st.beginGroup("offsets");
    for (auto it = m_offsets.cbegin(); it != m_offsets.cend(); ++it)
        st.setValue(it.key(), it.value());
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

QString KeyboardUi::offsetKey() const
{
    return m_shownPortrait ? QStringLiteral("portrait") : QStringLiteral("landscape");
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
    for (int i = 0; i < blocks.size(); ++i)
        m_windows[i]->setBlock(&blocks[i], pxPerMm());
    placeAll(); // the size may differ between layers
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
    }
    placeAll();

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
    if (onWayland() && !m_handle)
        createHandle();
    updateSpacer();
    updateVisibility();
}

void KeyboardUi::createHandle()
{
    const double ppm = pxPerMm();
    const double keyMm = m_layout ? m_layout->settings.keyMm : 15.0;
    m_handle = new HandleWindow;
    m_handle->resize(int(keyMm * 0.5 * ppm), int(keyMm * 0.9 * ppm));
    auto *lw = LayerShellQt::Window::get(m_handle);
    lw->setLayer(LayerShellQt::Window::LayerOverlay);
    lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    lw->setScope(QStringLiteral("thumbpad-handle"));
    lw->setExclusiveZone(-1);
    // Right edge, a third of the way up: clear of most apps' bottom-right buttons.
    lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom |
                                                 LayerShellQt::Window::AnchorRight));
    const QScreen *s = QGuiApplication::primaryScreen();
    const int h = s ? s->geometry().height() : 912;
    lw->setMargins(QMargins(0, 0, 0, h / 3));
    connect(m_handle, &HandleWindow::tapped, this, &KeyboardUi::show);
}

void KeyboardUi::placeAll()
{
    if (!m_layout)
        return;
    const QScreen *s = QGuiApplication::primaryScreen();
    const QSize screen = s ? s->geometry().size() : QSize(1368, 912);
    const double ppm = pxPerMm();
    const Settings &st = m_layout->settings;
    const double baseBottom = st.bottomMarginMm * ppm;

    // Clamp the shared offset so that every block stays fully on screen.
    double inLo = -1e9, inHi = 1e9, upLo = -1e9, upHi = 1e9;
    QList<double> bases;
    for (const auto &w : m_windows) {
        const bool center = w && w->block() && w->block()->edge == "center";
        const double base = !w ? 0 : center ? (screen.width() - w->width()) / 2.0
                                            : st.edgeMarginMm * ppm;
        bases << base;
        if (!w)
            continue;
        inLo = std::max(inLo, -base);
        inHi = std::min(inHi, screen.width() - w->width() - base);
        upLo = std::max(upLo, -baseBottom);
        upHi = std::min(upHi, screen.height() - w->height() - baseBottom);
    }
    QPointF &off = m_offsets[offsetKey()];
    off.setX(std::clamp(off.x(), std::min(inLo, inHi), inHi));
    off.setY(std::clamp(off.y(), std::min(upLo, upHi), upHi));

    if (!onWayland())
        return;
    for (int i = 0; i < m_windows.size(); ++i) {
        BlockWindow *w = m_windows[i];
        if (!w || !w->block())
            continue;
        const bool right = w->block()->edge == "right";
        const int side = int(bases[i] + off.x());
        const int bottom = int(baseBottom + off.y());
        auto *lw = LayerShellQt::Window::get(w);
        lw->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorBottom |
            (right ? LayerShellQt::Window::AnchorRight : LayerShellQt::Window::AnchorLeft)));
        lw->setMargins(right ? QMargins(0, 0, side, bottom) : QMargins(side, 0, 0, bottom));
        // Margins only reach the screen with the next surface commit, and Qt
        // only commits when it repaints: update() marks the window dirty
        // (requestUpdate() alone paints nothing, so nothing would move).
        w->update();
    }
}

void KeyboardUi::moveBlock(int index, QPointF delta)
{
    const BlockWindow *w = m_windows.value(index);
    if (!w || !w->block())
        return;
    // Screen delta -> (towards centre, up) for the dragged block.
    const double inward = w->block()->edge == "right" ? -delta.x() : delta.x();
    m_offsets[offsetKey()] += QPointF(inward, -delta.y());
    placeAll();
    updateSpacer();
}

void KeyboardUi::resetPositions()
{
    m_offsets.clear();
    placeAll();
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
    if (m_handle)
        m_handle->setVisible(!m_visible);
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
