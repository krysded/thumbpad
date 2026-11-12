#include "controller.h"
#include "dbusiface.h"
#include "inputmethod.h"
#include "keyboardui.h"
#include "layout.h"
#include "renderer.h"
#include "uinput.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <cstring>

using namespace tp;

namespace {

QString findLayout(const QString &explicitPath)
{
    if (!explicitPath.isEmpty())
        return explicitPath;
    const QString user =
        QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "thumbpad/layout.json");
    if (!user.isEmpty())
        return user;
    const QStringList candidates{
        QCoreApplication::applicationDirPath() + "/../layouts/default.json", // build tree
        QStringLiteral(THUMBPAD_DATADIR "/layouts/default.json"),
    };
    for (const QString &c : candidates)
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    return candidates.last();
}

std::shared_ptr<Layout> loadLayout(const QString &path)
{
    QStringList errors, warnings;
    auto l = Layout::load(path, &errors, &warnings);
    for (const QString &w : warnings)
        fprintf(stderr, "thumbpad: warning: %s\n", qPrintable(w));
    for (const QString &e : errors)
        fprintf(stderr, "thumbpad: error: %s\n", qPrintable(e));
    if (!l)
        return nullptr;
    return std::make_shared<Layout>(std::move(*l));
}

bool wantsOffscreen(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (!strcmp(argv[i], "--check") || !strcmp(argv[i], "--preview") ||
            !strncmp(argv[i], "--preview=", 10) || !strcmp(argv[i], "--toggle"))
            return true;
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    // Tooling modes must work without a compositor (e.g. over ssh).
    if (wantsOffscreen(argc, argv))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("thumbpad"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Swipe-gesture on-screen keyboard"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption layoutOpt({"l", "layout"}, "Layout JSON file.", "file");
    QCommandLineOption checkOpt("check", "Validate the layout, print a summary and exit.");
    QCommandLineOption previewOpt("preview", "Render every layer to a PNG and exit.", "png");
    QCommandLineOption showOpt("show", "Stay visible instead of waiting for a text field.");
    QCommandLineOption toggleOpt("toggle", "Toggle the running instance and exit.");
    parser.addOptions({layoutOpt, checkOpt, previewOpt, showOpt, toggleOpt});
    parser.process(app);

    if (parser.isSet(toggleOpt)) {
        QDBusInterface iface(kDBusService, kDBusPath, kDBusInterface);
        QDBusReply<void> r = iface.call(QStringLiteral("Toggle"));
        if (!r.isValid()) {
            fprintf(stderr, "thumbpad: no running instance: %s\n", qPrintable(r.error().message()));
            return 1;
        }
        return 0;
    }

    const QString layoutPath = findLayout(parser.value(layoutOpt));
    std::shared_ptr<Layout> layout = loadLayout(layoutPath);
    if (!layout) {
        fprintf(stderr, "thumbpad: cannot load layout %s\n", qPrintable(layoutPath));
        return 1;
    }

    if (parser.isSet(checkOpt)) {
        printf("%s", qPrintable(layout->summary()));
        return 0;
    }
    if (parser.isSet(previewOpt)) {
        QString err;
        if (!renderPreview(*layout, parser.value(previewOpt), &err)) {
            fprintf(stderr, "thumbpad: %s\n", qPrintable(err));
            return 1;
        }
        printf("wrote %s\n", qPrintable(parser.value(previewOpt)));
        return 0;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kDBusService)) {
        fprintf(stderr, "thumbpad: already running (use --toggle)\n");
        return 1;
    }

    UInput uinput;
    QString uerr;
    if (!uinput.open(&uerr))
        fprintf(stderr, "thumbpad: warning: %s; Ctrl/Alt/Esc/arrows won't work\n", qPrintable(uerr));

    InputMethod *im = nullptr;
    if (QGuiApplication::platformName().startsWith("wayland")) {
        im = new InputMethod();
        QObject::connect(im, &QWaylandClientExtension::activeChanged, im, [im] {
            fprintf(stderr, "thumbpad: input method protocol %s\n",
                    im->isActive() ? "bound" : "lost");
        });
        QTimer::singleShot(1500, im, [im] {
            if (!im->isActive())
                fprintf(stderr, "thumbpad: input method protocol unavailable (not launched by "
                                "KWin?), text falls back to ASCII via uinput\n");
        });
    }

    Controller controller(im, &uinput);
    KeyboardUi ui(&controller);
    ui.setLayout(layout);
    if (im)
        QObject::connect(im, &InputMethod::contextChanged, &ui, &KeyboardUi::setImActive);

    auto reload = [&ui, layoutPath] {
        if (auto l = loadLayout(layoutPath)) {
            ui.setLayout(l);
            fprintf(stderr, "thumbpad: reloaded %s\n", qPrintable(layoutPath));
        }
    };
    DBusIface dbus(&controller, &ui, reload, &app);
    bus.registerObject(kDBusPath, &dbus, QDBusConnection::ExportAllSlots);

    // Live reload while editing the layout. Editors often replace the file,
    // which drops the watch, so re-add it each time.
    QFileSystemWatcher watcher({layoutPath});
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(200);
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &debounce,
                     qOverload<>(&QTimer::start));
    QObject::connect(&debounce, &QTimer::timeout, &app, [&watcher, layoutPath, reload] {
        if (!watcher.files().contains(layoutPath))
            watcher.addPath(layoutPath);
        reload();
    });

    if (parser.isSet(showOpt))
        ui.show();
    fprintf(stderr, "thumbpad: running with layout %s\n", qPrintable(layoutPath));
    const int rc = app.exec();
    delete im;
    return rc;
}
