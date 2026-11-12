#pragma once

#include "layout.h"

#include <QList>
#include <QPainter>
#include <QRectF>
#include <array>

namespace tp {

struct RenderState {
    bool shifted = false;
    std::array<int, ModCount> mods{}; // 0 off, 1 one-shot, 2 locked
    struct Pressed {
        int row, col;
        int dir; // -1 = centre (tap), else Dir
    };
    QList<Pressed> pressed;
};

// Key rectangles in block coordinates, indexed [row][col].
QList<QList<QRectF>> keyRects(const Block &b, double keyPx);

void paintBlock(QPainter &p, const Block &b, double keyPx, double gapPx,
                const RenderState &st, double opacity);

// Draws every layer in both orientations on a to-scale mock screen (PNG).
bool renderPreview(const Layout &l, const QString &path, QString *error);

} // namespace tp
