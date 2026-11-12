#include "renderer.h"

#include <QFont>
#include <QImage>
#include <QPainterPath>

namespace tp {

namespace {

const QColor kBlockBg(20, 21, 25);
const QColor kKey(43, 46, 54);
const QColor kKeyPressed(74, 82, 99);
const QColor kKeyUtility(33, 36, 43);
const QColor kText(240, 240, 240);
const QColor kSwipeText(154, 163, 178);
const QColor kLongText(110, 118, 132);
const QColor kHighlight(255, 209, 102);
const QColor kModOneShot(76, 141, 255);
const QColor kModLocked(255, 159, 67);

// Unit offsets of swipe labels from the key centre, indexed by Dir.
const QPointF kDirOffset[DirCount] = {{0, -1}, {1, -1}, {1, 0}, {1, 1},
                                      {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};

bool isUtility(const KeyDef &k)
{
    return k.tap.type != Action::Type::Text || k.cursorSlide;
}

QColor modColor(const Action &a, const RenderState &st, const QColor &fallback)
{
    if (a.type != Action::Type::Mod)
        return fallback;
    const int s = st.mods[int(a.mod)];
    return s == 2 ? kModLocked : s == 1 ? kModOneShot : fallback;
}

void drawText(QPainter &p, const QPointF &centre, double px, const QString &text,
              const QColor &color, bool bold = false)
{
    if (text.isEmpty())
        return;
    QFont f = p.font();
    // Long labels ("Home", "C-⌫") shrink so they stay inside the key.
    const double scale = text.size() > 2 ? 0.62 : 1.0;
    f.setPixelSize(std::max(6, int(px * scale)));
    f.setBold(bold);
    p.setFont(f);
    p.setPen(color);
    const QRectF box(centre.x() - px * 2, centre.y() - px, px * 4, px * 2);
    p.drawText(box, Qt::AlignCenter, text);
}

} // namespace

QList<QList<QRectF>> keyRects(const Block &b, double keyPx)
{
    QList<QList<QRectF>> out;
    for (int r = 0; r < b.rows.size(); ++r) {
        QList<QRectF> row;
        double x = 0;
        for (const KeyDef &k : b.rows[r]) {
            row << QRectF(x, r * keyPx, k.width * keyPx, keyPx);
            x += k.width * keyPx;
        }
        out << row;
    }
    return out;
}

void paintBlock(QPainter &p, const Block &b, double keyPx, double gapPx, const RenderState &st,
                double opacity)
{
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF all(0, 0, b.columns() * keyPx, b.rowCount() * keyPx);
    QColor bg = kBlockBg;
    bg.setAlphaF(opacity);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(all, keyPx * 0.15, keyPx * 0.15);

    const auto rects = keyRects(b, keyPx);
    for (int r = 0; r < b.rows.size(); ++r) {
        for (int c = 0; c < b.rows[r].size(); ++c) {
            const KeyDef &k = b.rows[r][c];
            if (k.empty)
                continue;
            const QRectF rect = rects[r][c].adjusted(gapPx / 2, gapPx / 2, -gapPx / 2, -gapPx / 2);

            int pressedDir = -2; // -2 = not pressed
            for (const auto &pr : st.pressed)
                if (pr.row == r && pr.col == c)
                    pressedDir = pr.dir;

            QColor fill = pressedDir != -2 ? kKeyPressed : isUtility(k) ? kKeyUtility : kKey;
            fill = modColor(k.tap, st, fill);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRoundedRect(rect, keyPx * 0.12, keyPx * 0.12);

            const QPointF centre = rect.center();
            const QString tapLabel =
                !k.label.isEmpty() ? k.label : displayLabel(k.tap, st.shifted);
            drawText(p, centre, keyPx * 0.36, tapLabel,
                     pressedDir == -1 ? kHighlight : kText, pressedDir == -1);

            const double off = keyPx * 0.33;
            for (int d = 0; d < DirCount; ++d) {
                const Action &a = k.swipes[d];
                if (!a.isValid())
                    continue;
                const QPointF pos = centre + kDirOffset[d] * off;
                const bool hl = pressedDir == d;
                drawText(p, pos, keyPx * 0.17, displayLabel(a, st.shifted),
                         hl ? kHighlight : modColor(a, st, kSwipeText), hl);
            }
            if (k.longPress.isValid()) {
                // Between the centre and the top-right swipe label.
                const QPointF pos = centre + QPointF(keyPx * 0.17, -keyPx * 0.17);
                drawText(p, pos, keyPx * 0.12, displayLabel(k.longPress, st.shifted), kLongText);
            }
        }
    }
}

bool renderPreview(const Layout &l, const QString &path, QString *error)
{
    // Surface Pro 6 panel: 260 x 173 mm.
    const QSizeF landscapeMm(260, 173);
    const double s = 3.5; // px per mm in the preview
    const double margin = 30, title = 34;
    const QSizeF land = landscapeMm * s;
    const QSizeF port(land.height(), land.width());
    const double cellH = title + std::max(land.height(), port.height()) + margin;

    QStringList order = l.layers.keys();
    order.removeAll("main");
    order.prepend("main");

    QImage img(int(margin * 3 + land.width() + port.width()), int(margin + cellH * order.size()),
               QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(245, 245, 247));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    const Settings &st = l.settings;
    const double keyPx = st.keyMm * s, gapPx = st.gapMm * s;
    for (int li = 0; li < order.size(); ++li) {
        const LayerDef &layer = l.layers.value(order[li]);
        const double top = margin + li * cellH;
        for (bool portrait : {false, true}) {
            const QSizeF scr = portrait ? port : land;
            const double left = portrait ? margin * 2 + land.width() : margin;
            QFont f = p.font();
            f.setPixelSize(18);
            f.setBold(true);
            p.setFont(f);
            p.setPen(QColor(40, 40, 40));
            p.drawText(QPointF(left, top + 22),
                       QStringLiteral("%1 — layer \"%2\" — %3")
                           .arg(l.name, order[li], portrait ? "portrait" : "landscape"));
            const QRectF screen(left, top + title, scr.width(), scr.height());
            p.setPen(QPen(QColor(60, 60, 60), 3));
            p.setBrush(QColor(200, 205, 215));
            p.drawRoundedRect(screen, 8, 8);

            for (const Block &b : layer.blocks(portrait)) {
                const QSizeF size(b.columns() * keyPx, b.rowCount() * keyPx);
                double x = screen.left() + (screen.width() - size.width()) / 2;
                if (b.edge == "left")
                    x = screen.left() + st.edgeMarginMm * s;
                else if (b.edge == "right")
                    x = screen.right() - st.edgeMarginMm * s - size.width();
                const double y = screen.bottom() - st.bottomMarginMm * s - size.height();
                p.save();
                p.translate(x, y);
                paintBlock(p, b, keyPx, gapPx, RenderState{}, st.opacity);
                p.restore();
            }
        }
    }
    p.end();
    if (!img.save(path)) {
        *error = QStringLiteral("cannot write %1").arg(path);
        return false;
    }
    return true;
}

} // namespace tp
