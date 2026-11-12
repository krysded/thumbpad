#include "layout.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace tp {

namespace {

const char *const kDirNames[DirCount] = {"t", "tr", "r", "br", "b", "bl", "l", "tl"};
const char *const kDirLongNames[DirCount] = {"top", "topRight", "right", "bottomRight",
                                             "bottom", "bottomLeft", "left", "topLeft"};

int dirFromName(const QString &n)
{
    for (int i = 0; i < DirCount; ++i)
        if (n == QLatin1String(kDirNames[i]) || n == QLatin1String(kDirLongNames[i]))
            return i;
    return -1;
}

bool checkFields(const QJsonObject &o, const QSet<QString> &allowed, const QString &where,
                 QStringList *errors)
{
    bool ok = true;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!allowed.contains(it.key())) {
            *errors << QStringLiteral("%1: unknown field \"%2\"").arg(where, it.key());
            ok = false;
        }
    }
    return ok;
}

Action actionAt(const QJsonValue &v, const QString &where, QStringList *errors)
{
    QString err;
    Action a = parseAction(v, &err);
    if (!err.isEmpty())
        *errors << QStringLiteral("%1: %2").arg(where, err);
    return a;
}

KeyDef parseKey(const QJsonValue &v, const QString &where, QStringList *errors)
{
    KeyDef k;
    if (v.isNull()) {
        k.empty = true;
        return k;
    }
    if (v.isString()) { // shorthand: "x" == {"tap": "x"}
        k.tap = actionAt(v, where, errors);
        return k;
    }
    if (!v.isObject()) {
        *errors << QStringLiteral("%1: key must be an object, a string or null").arg(where);
        return k;
    }
    const QJsonObject o = v.toObject();
    checkFields(o, {"tap", "long", "swipe", "repeat", "slide", "w", "label", "gap"}, where,
                errors);
    if (o.contains("gap")) { // {"gap": 0.5} is an empty cell of that width
        k.empty = true;
        k.width = o.value("gap").toDouble(1.0);
        return k;
    }
    k.tap = actionAt(o.value("tap"), where + ".tap", errors);
    k.longPress = actionAt(o.value("long"), where + ".long", errors);
    k.repeat = o.value("repeat").toBool(false);
    k.label = o.value("label").toString();
    k.width = o.value("w").toDouble(1.0);
    if (k.width <= 0)
        *errors << QStringLiteral("%1: width must be positive").arg(where);
    if (o.contains("slide")) {
        if (o.value("slide").toString() == "cursor")
            k.cursorSlide = true;
        else
            *errors << QStringLiteral("%1: slide must be \"cursor\"").arg(where);
    }
    const QJsonObject sw = o.value("swipe").toObject();
    for (auto it = sw.begin(); it != sw.end(); ++it) {
        const int d = dirFromName(it.key());
        if (d < 0) {
            *errors << QStringLiteral("%1.swipe: unknown direction \"%2\"").arg(where, it.key());
            continue;
        }
        if (k.swipes[d].isValid())
            *errors << QStringLiteral("%1.swipe: direction \"%2\" given twice").arg(where, it.key());
        k.swipes[d] = actionAt(it.value(), where + ".swipe." + it.key(), errors);
    }
    if (k.cursorSlide) {
        for (const Action &s : k.swipes)
            if (s.isValid())
                *errors << QStringLiteral("%1: a cursor-slide key can't have swipes").arg(where);
    }
    if (!k.tap.isValid() && !k.longPress.isValid()) {
        bool any = false;
        for (const Action &s : k.swipes)
            any |= s.isValid();
        if (!any)
            *errors << QStringLiteral("%1: key does nothing (use null for a gap)").arg(where);
    }
    return k;
}

QList<Block> parseBlocks(const QJsonValue &v, const QString &where, QStringList *errors)
{
    QList<Block> blocks;
    if (v.isUndefined())
        return blocks;
    if (!v.isArray()) {
        *errors << QStringLiteral("%1: must be an array of blocks").arg(where);
        return blocks;
    }
    const QJsonArray arr = v.toArray();
    for (int bi = 0; bi < arr.size(); ++bi) {
        const QString bw = QStringLiteral("%1[%2]").arg(where).arg(bi);
        const QJsonObject bo = arr.at(bi).toObject();
        checkFields(bo, {"edge", "rows"}, bw, errors);
        Block b;
        b.edge = bo.value("edge").toString("center");
        if (b.edge != "left" && b.edge != "right" && b.edge != "center")
            *errors << QStringLiteral("%1: edge must be left, right or center").arg(bw);
        const QJsonArray rows = bo.value("rows").toArray();
        if (rows.isEmpty())
            *errors << QStringLiteral("%1: no rows").arg(bw);
        for (int r = 0; r < rows.size(); ++r) {
            QList<KeyDef> row;
            const QJsonArray cells = rows.at(r).toArray();
            for (int c = 0; c < cells.size(); ++c)
                row << parseKey(cells.at(c), QStringLiteral("%1.r%2c%3").arg(bw).arg(r).arg(c),
                                errors);
            b.rows << row;
        }
        blocks << b;
    }
    return blocks;
}

void collectActions(const LayerDef &layer, QList<const Action *> *out)
{
    for (const QList<Block> *list : {&layer.landscape, &layer.portrait})
        for (const Block &b : *list)
            for (const auto &row : b.rows)
                for (const KeyDef &k : row) {
                    *out << &k.tap << &k.longPress;
                    for (const Action &s : k.swipes)
                        *out << &s;
                }
}

QString describe(const Action &a)
{
    switch (a.type) {
    case Action::Type::None: return {};
    case Action::Type::Text: {
        QString t = a.text;
        t.replace("\n", "\\n").replace("\t", "\\t");
        if (t == " ") t = "space";
        return t;
    }
    case Action::Type::Key: return "[" + a.keySpec + "]";
    case Action::Type::Mod: return "[mod " + modName(a.mod) + "]";
    case Action::Type::Layer: return "[layer " + a.layer + "]";
    case Action::Type::Hide: return "[hide]";
    }
    return {};
}

} // namespace

const char *dirName(int d)
{
    return (d >= 0 && d < DirCount) ? kDirNames[d] : "?";
}

double Block::columns() const
{
    double maxW = 0;
    for (const auto &row : rows) {
        double w = 0;
        for (const KeyDef &k : row)
            w += k.width;
        maxW = std::max(maxW, w);
    }
    return maxW;
}

std::optional<Layout> Layout::fromJson(const QByteArray &json, QStringList *errors,
                                       QStringList *warnings)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError) {
        // Report a line number: offsets alone are useless when editing by hand.
        const int line = int(json.left(pe.offset).count('\n')) + 1;
        *errors << QStringLiteral("JSON syntax error at line %1: %2").arg(line).arg(pe.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    const int errorsBefore = int(errors->size());
    checkFields(root, {"name", "settings", "layers", "comment"}, "layout", errors);

    Layout l;
    l.name = root.value("name").toString("unnamed");

    const QJsonObject s = root.value("settings").toObject();
    checkFields(s, {"keyMm", "gapMm", "swipeThresholdMm", "slideStepMm", "edgeMarginMm",
                    "bottomMarginMm", "longPressMs", "repeatDelayMs", "repeatIntervalMs",
                    "opacity", "requiredChars"},
                "settings", errors);
    Settings &st = l.settings;
    st.keyMm = s.value("keyMm").toDouble(st.keyMm);
    st.gapMm = s.value("gapMm").toDouble(st.gapMm);
    st.swipeThresholdMm = s.value("swipeThresholdMm").toDouble(st.swipeThresholdMm);
    st.slideStepMm = s.value("slideStepMm").toDouble(st.slideStepMm);
    st.edgeMarginMm = s.value("edgeMarginMm").toDouble(st.edgeMarginMm);
    st.bottomMarginMm = s.value("bottomMarginMm").toDouble(st.bottomMarginMm);
    st.longPressMs = s.value("longPressMs").toInt(st.longPressMs);
    st.repeatDelayMs = s.value("repeatDelayMs").toInt(st.repeatDelayMs);
    st.repeatIntervalMs = s.value("repeatIntervalMs").toInt(st.repeatIntervalMs);
    st.opacity = s.value("opacity").toDouble(st.opacity);
    st.requiredChars = s.value("requiredChars").toString();

    const QJsonObject layers = root.value("layers").toObject();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        const QString lw = "layers." + it.key();
        const QJsonObject lo = it.value().toObject();
        checkFields(lo, {"landscape", "portrait"}, lw, errors);
        LayerDef ld;
        ld.landscape = parseBlocks(lo.value("landscape"), lw + ".landscape", errors);
        ld.portrait = parseBlocks(lo.value("portrait"), lw + ".portrait", errors);
        if (ld.landscape.isEmpty())
            *errors << lw + ": needs at least one landscape block";
        if (ld.portrait.isEmpty())
            *warnings << lw + ": no portrait blocks, portrait will reuse landscape";
        l.layers.insert(it.key(), ld);
    }
    if (!l.layers.contains("main"))
        *errors << QStringLiteral("layers: a \"main\" layer is required");

    // Cross-checks: layer references, duplicates, coverage.
    for (auto it = l.layers.cbegin(); it != l.layers.cend(); ++it) {
        QList<const Action *> actions;
        collectActions(it.value(), &actions);
        for (const Action *a : actions)
            if (a->type == Action::Type::Layer && !l.layers.contains(a->layer))
                *errors << QStringLiteral("layers.%1: switches to unknown layer \"%2\"")
                               .arg(it.key(), a->layer);
    }
    for (bool portrait : {false, true}) {
        const LayerDef &main = l.layers.value("main");
        if (portrait && main.portrait.isEmpty())
            continue;
        QHash<QString, int> seen;
        for (const Block &b : main.blocks(portrait))
            for (const auto &row : b.rows)
                for (const KeyDef &k : row) {
                    QList<const Action *> acts{&k.tap, &k.longPress};
                    for (const Action &s : k.swipes)
                        acts << &s;
                    for (const Action *a : acts)
                        if (a->type == Action::Type::Text)
                            seen[a->text]++;
                }
        const QString orient = portrait ? "portrait" : "landscape";
        for (auto it = seen.cbegin(); it != seen.cend(); ++it)
            if (it.value() > 1)
                *warnings << QStringLiteral("main/%1: \"%2\" appears %3 times")
                                 .arg(orient, it.key()).arg(it.value());
        QString missing;
        for (const QChar c : st.requiredChars)
            if (!c.isSpace() && !seen.contains(QString(c)))
                missing += c;
        if (!missing.isEmpty())
            *warnings << QStringLiteral("main/%1: unreachable required characters: %2")
                             .arg(orient, missing);
    }

    if (errors->size() > errorsBefore)
        return std::nullopt;
    return l;
}

std::optional<Layout> Layout::load(const QString &path, QStringList *errors, QStringList *warnings)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *errors << QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        return std::nullopt;
    }
    return fromJson(f.readAll(), errors, warnings);
}

QString Layout::summary() const
{
    QString out;
    out += QStringLiteral("Layout: %1\n").arg(name);
    for (auto it = layers.cbegin(); it != layers.cend(); ++it) {
        for (bool portrait : {false, true}) {
            if (portrait && it.value().portrait.isEmpty())
                continue;
            const QList<Block> &blocks = it.value().blocks(portrait);
            for (int bi = 0; bi < blocks.size(); ++bi) {
                const Block &b = blocks.at(bi);
                out += QStringLiteral("\n== layer %1 | %2 | block %3 (edge %4, %5x%6)\n")
                           .arg(it.key(), portrait ? "portrait" : "landscape")
                           .arg(bi).arg(b.edge).arg(b.columns()).arg(b.rowCount());
                for (int r = 0; r < b.rows.size(); ++r) {
                    for (int c = 0; c < b.rows[r].size(); ++c) {
                        const KeyDef &k = b.rows[r][c];
                        if (k.empty)
                            continue;
                        QString line = QStringLiteral("  r%1c%2  tap %3").arg(r).arg(c)
                                           .arg(describe(k.tap), -8);
                        if (k.longPress.isValid())
                            line += "  long " + describe(k.longPress);
                        for (int d = 0; d < DirCount; ++d)
                            if (k.swipes[d].isValid())
                                line += QStringLiteral("  %1 %2").arg(dirName(d), describe(k.swipes[d]));
                        if (k.repeat) line += "  (repeat)";
                        if (k.cursorSlide) line += "  (cursor slide)";
                        out += line + "\n";
                    }
                }
            }
        }
    }
    return out;
}

} // namespace tp
