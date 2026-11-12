#include "blockwindow.h"
#include "controller.h"
#include "gesture.h"
#include "renderer.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
#include <cmath>
#include <linux/input-event-codes.h>

namespace tp {

namespace {
constexpr int kMouseId = -1;
}

BlockWindow::BlockWindow(Controller *controller, const Settings &settings)
    : m_controller(controller), m_settings(settings)
{
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setTitle(QStringLiteral("thumbpad"));
}

void BlockWindow::setBlock(const Block *block, double pxPerMm)
{
    m_block = block;
    m_pxPerMm = pxPerMm;
    if (m_block)
        resize(int(std::ceil(m_block->columns() * keyPx())),
               int(std::ceil(m_block->rowCount() * keyPx())));
    update();
}

void BlockWindow::cancelAll()
{
    m_presses.clear();
    update();
}

bool BlockWindow::hitTest(QPointF pos, int *row, int *col) const
{
    if (!m_block)
        return false;
    const auto rects = keyRects(*m_block, keyPx());
    for (int r = 0; r < rects.size(); ++r)
        for (int c = 0; c < rects[r].size(); ++c)
            if (rects[r][c].contains(pos) && !m_block->rows[r][c].empty) {
                *row = r;
                *col = c;
                return true;
            }
    return false;
}

void BlockWindow::begin(int id, QPointF pos)
{
    int r, c;
    if (!hitTest(pos, &r, &c))
        return;
    auto p = std::make_unique<Press>();
    p->row = r;
    p->col = c;
    p->key = &m_block->rows[r][c];
    p->start = p->slideRef = pos;
    p->timer = std::make_unique<QTimer>();
    p->timer->setSingleShot(true);

    Press *raw = p.get();
    const KeyDef *key = raw->key;
    if (key->repeat) {
        p->timer->setInterval(m_settings.repeatDelayMs);
        connect(p->timer.get(), &QTimer::timeout, this, [this, raw, key] {
            if (raw->moved)
                return;
            raw->consumed = true;
            m_controller->execute(key->tap);
            raw->timer->setSingleShot(false);
            raw->timer->start(m_settings.repeatIntervalMs);
        });
        p->timer->start();
    } else if (key->longPress.isValid()) {
        p->timer->setInterval(m_settings.longPressMs);
        connect(p->timer.get(), &QTimer::timeout, this, [this, raw, key] {
            if (raw->moved)
                return;
            raw->consumed = true;
            m_controller->execute(key->longPress);
            update();
        });
        p->timer->start();
    }
    m_presses[id] = std::move(p);
    update();
}

void BlockWindow::move(int id, QPointF pos)
{
    auto it = m_presses.find(id);
    if (it == m_presses.end())
        return;
    Press &p = *it->second;
    const QPointF d = pos - p.start;
    const double dist = std::hypot(d.x(), d.y());
    const double threshold = m_settings.swipeThresholdMm * m_pxPerMm;

    if (p.key->cursorSlide) {
        if (!p.sliding && dist > threshold) {
            p.sliding = p.moved = true;
            p.timer->stop();
            p.slideRef = pos;
        }
        if (p.sliding) {
            // Horizontal steps move by character; vertical ones need a longer drag.
            const double hStep = m_settings.slideStepMm * m_pxPerMm;
            const double vStep = hStep * 2;
            while (pos.x() - p.slideRef.x() >= hStep) {
                m_controller->tapKey(KEY_RIGHT);
                p.slideRef.rx() += hStep;
            }
            while (p.slideRef.x() - pos.x() >= hStep) {
                m_controller->tapKey(KEY_LEFT);
                p.slideRef.rx() -= hStep;
            }
            while (pos.y() - p.slideRef.y() >= vStep) {
                m_controller->tapKey(KEY_DOWN);
                p.slideRef.ry() += vStep;
            }
            while (p.slideRef.y() - pos.y() >= vStep) {
                m_controller->tapKey(KEY_UP);
                p.slideRef.ry() -= vStep;
            }
        }
        return;
    }

    const int oldDir = p.dir;
    if (dist > threshold) {
        if (!p.consumed) {
            p.moved = true;
            p.timer->stop();
        }
        p.dir = classifyDirection(d.x(), d.y());
    } else {
        p.dir = -1;
    }
    if (p.dir != oldDir)
        update();
}

void BlockWindow::end(int id, QPointF pos)
{
    move(id, pos);
    auto it = m_presses.find(id);
    if (it == m_presses.end())
        return;
    // Take ownership first: executing may switch layers or hide this window.
    std::unique_ptr<Press> p = std::move(it->second);
    m_presses.erase(it);
    p->timer->stop();
    update();

    if (p->consumed || p->sliding)
        return;
    if (p->dir < 0)
        m_controller->execute(p->key->tap);
    else
        m_controller->execute(p->key->swipes[p->dir]);
}

bool BlockWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto *te = static_cast<QTouchEvent *>(e);
        for (const QEventPoint &pt : te->points()) {
            switch (pt.state()) {
            case QEventPoint::Pressed: begin(pt.id(), pt.position()); break;
            case QEventPoint::Updated: move(pt.id(), pt.position()); break;
            case QEventPoint::Released: end(pt.id(), pt.position()); break;
            default: break;
            }
        }
        e->accept();
        return true;
    }
    case QEvent::TouchCancel:
        cancelAll();
        e->accept();
        return true;
    default:
        return QRasterWindow::event(e);
    }
}

void BlockWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        begin(kMouseId, e->position());
}

void BlockWindow::mouseMoveEvent(QMouseEvent *e)
{
    move(kMouseId, e->position());
}

void BlockWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        end(kMouseId, e->position());
}

void BlockWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(QPoint(), size()), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (!m_block)
        return;
    RenderState st = m_controller->renderState();
    for (const auto &[id, press] : m_presses)
        st.pressed << RenderState::Pressed{press->row, press->col,
                                           press->sliding ? -1 : press->dir};
    paintBlock(p, *m_block, keyPx(), m_settings.gapMm * m_pxPerMm, st, m_settings.opacity);
}

} // namespace tp
