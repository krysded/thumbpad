#pragma once

#include "layout.h"

#include <QRasterWindow>
#include <QTimer>
#include <map>
#include <memory>

namespace tp {

class Controller;

// One on-screen block (e.g. the left-thumb half). Tracks each touch point
// independently, so both thumbs can type at the same time.
class BlockWindow : public QRasterWindow
{
    Q_OBJECT
public:
    BlockWindow(Controller *controller, const Settings &settings);

    void setBlock(const Block *block, double pxPerMm);
    const Block *block() const { return m_block; }
    void cancelAll();

Q_SIGNALS:
    // Asks the owner to move this window by delta (logical px, screen axes).
    void moveRequested(BlockWindow *self, QPointF delta);
    void moveFinished();

protected:
    bool event(QEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    struct Press {
        int row = -1, col = -1;
        const KeyDef *key = nullptr;
        QPointF start, slideRef;
        int dir = -1;          // -1 = centre
        bool moved = false;    // went past the swipe threshold at some point
        bool consumed = false; // long-press or repeat already acted
        bool sliding = false;  // cursor slide or block move in progress
        std::unique_ptr<QTimer> timer;
    };

    void begin(int id, QPointF pos);
    void move(int id, QPointF pos);
    void end(int id, QPointF pos);
    bool hitTest(QPointF pos, int *row, int *col) const;
    double keyPx() const { return m_settings.keyMm * m_pxPerMm; }

    Controller *m_controller;
    Settings m_settings;
    const Block *m_block = nullptr;
    double m_pxPerMm = 96.0 / 25.4;
    std::map<int, std::unique_ptr<Press>> m_presses;
    bool m_moveInFlight = false;  // waiting for a block move to reach the screen
    bool m_moveCommitted = false;
    QTimer m_moveTimeout;
};

} // namespace tp
