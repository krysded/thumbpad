#pragma once

#include "actions.h"

#include <QMap>
#include <QStringList>
#include <array>
#include <optional>

namespace tp {

// Swipe directions in clockwise order starting at the top.
enum Dir { T = 0, TR, R, BR, B, BL, L, TL };
constexpr int DirCount = 8;
const char *dirName(int d); // "t", "tr", ...

struct KeyDef {
    bool empty = false;     // a gap (JSON null)
    double width = 1.0;     // in key units
    Action tap;
    Action longPress;
    std::array<Action, DirCount> swipes;
    bool repeat = false;      // hold repeats the tap action (backspace, arrows)
    bool cursorSlide = false; // dragging moves the cursor (spacebar)
    bool moveSlide = false;   // dragging moves this block on screen
    QString label;            // overrides the tap label
};

struct Block {
    QString edge; // "left", "right" or "center"
    QList<QList<KeyDef>> rows;

    double columns() const; // widest row, in key units
    int rowCount() const { return int(rows.size()); }
};

struct LayerDef {
    QList<Block> landscape;
    QList<Block> portrait; // falls back to landscape when empty
    const QList<Block> &blocks(bool portraitMode) const
    {
        return portraitMode && !portrait.isEmpty() ? portrait : landscape;
    }
};

struct Settings {
    double keyMm = 15.0;
    double gapMm = 1.2;
    double swipeThresholdMm = 3.5; // movement below this is a tap
    double slideStepMm = 2.5;      // cursor slide: one arrow press per step
    double edgeMarginMm = 2.0;
    double bottomMarginMm = 2.0;
    int longPressMs = 450;
    int repeatDelayMs = 400;
    int repeatIntervalMs = 50;
    double opacity = 0.92;
    QString requiredChars; // coverage check: warn when one is unreachable
};

struct Layout {
    QString name;
    Settings settings;
    QMap<QString, LayerDef> layers;

    // errors make loading fail; warnings are informational.
    static std::optional<Layout> fromJson(const QByteArray &json, QStringList *errors,
                                          QStringList *warnings);
    static std::optional<Layout> load(const QString &path, QStringList *errors,
                                      QStringList *warnings);

    // Human-readable table of every key, for review before deploying.
    QString summary() const;
};

} // namespace tp
