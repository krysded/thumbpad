// Minimal self-contained tests for the platform-independent core.
#include "actions.h"
#include "gesture.h"
#include "layout.h"

#include <QJsonObject>
#include <cstdio>
#include <linux/input-event-codes.h>

using namespace tp;

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static void testDirections()
{
    CHECK(classifyDirection(10, 0) == R);
    CHECK(classifyDirection(-10, 0) == L);
    CHECK(classifyDirection(0, -10) == T);
    CHECK(classifyDirection(0, 10) == B);
    CHECK(classifyDirection(10, -10) == TR);
    CHECK(classifyDirection(-10, -10) == TL);
    CHECK(classifyDirection(10, 10) == BR);
    CHECK(classifyDirection(-10, 10) == BL);
    // Sector boundaries: 20° above horizontal is still R, 25° is TR.
    CHECK(classifyDirection(10, -3.64) == R);
    CHECK(classifyDirection(10, -4.66) == TR);
}

static void testActions()
{
    QString err;
    Action a = parseAction(QJsonValue("ě"), &err);
    CHECK(err.isEmpty() && a.type == Action::Type::Text && a.text == "ě");

    a = parseAction(QJsonValue(QJsonObject{{"key", "ctrl+shift+c"}}), &err);
    CHECK(err.isEmpty() && a.type == Action::Type::Key);
    CHECK((a.keys == QList<int>{KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_C}));
    CHECK(displayLabel(a, false) == "C-S-c");

    err.clear();
    parseAction(QJsonValue(QJsonObject{{"key", "ctrl+nosuchkey"}}), &err);
    CHECK(!err.isEmpty());

    err.clear();
    parseAction(QJsonValue(QJsonObject{{"key", "esc"}, {"mod", "ctrl"}}), &err);
    CHECK(!err.isEmpty()); // two kinds in one object

    err.clear();
    a = parseAction(QJsonValue(QJsonObject{{"mod", "alt"}}), &err);
    CHECK(err.isEmpty() && a.type == Action::Type::Mod && a.mod == Mod::Alt);

    // "@" and other symbols are plain text, never syntax.
    a = parseAction(QJsonValue("@"), &err);
    CHECK(a.type == Action::Type::Text && a.text == "@");

    CHECK(displayLabel(parseAction(QJsonValue("́"), &err), false) == "◌́");
    CHECK(displayLabel(parseAction(QJsonValue("ž"), &err), true) == "Ž");

    int code;
    bool shift;
    CHECK(asciiToKey('A', &code, &shift) && code == KEY_A && shift);
    CHECK(asciiToKey('"', &code, &shift) && code == KEY_APOSTROPHE && shift);
    CHECK(!asciiToKey(QChar(0x0161), &code, &shift)); // š
}

static void testLayoutErrors()
{
    QStringList errors, warnings;
    auto l = Layout::fromJson(R"({"layers": {"main": {"landscape": [
        {"edge": "left", "rows": [["a", {"tap": "b", "swipe": {"up": "c"}}]]}]}}})",
                              &errors, &warnings);
    CHECK(!l); // "up" is not a direction
    CHECK(!errors.isEmpty());

    errors.clear();
    l = Layout::fromJson(R"({"layers": {"main": {"landscape": [
        {"edge": "left", "rows": [["a", {"tap": {"layer": "nope"}}]]}]}}})",
                         &errors, &warnings);
    CHECK(!l); // unknown layer

    errors.clear();
    l = Layout::fromJson("{\n\"layers\": {,}\n}", &errors, &warnings);
    CHECK(!l && !errors.isEmpty() && errors.first().contains("line 2"));
}

static void testDefaultLayout()
{
    QStringList errors, warnings;
    auto l = Layout::load(LAYOUT_DIR "/default.json", &errors, &warnings);
    for (const QString &e : errors)
        fprintf(stderr, "  layout error: %s\n", qPrintable(e));
    CHECK(l.has_value());
    if (!l)
        return;
    for (const QString &w : warnings)
        CHECK(!w.contains("unreachable")); // every required character is placed
    CHECK(l->layers.contains("numeric"));
    CHECK(l->layers.value("main").landscape.size() == 2);
}

int main()
{
    testDirections();
    testActions();
    testLayoutErrors();
    testDefaultLayout();
    if (g_failures == 0)
        printf("all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
