#include "controller.h"
#include "dbusiface.h"
#include "inputmethod.h"
#include "keyboardui.h"
#include "layout.h"
#include "renderer.h"
#include "settingswindow.h"
#include "uinput.h"
#include "usersettings.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QQuickStyle>
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
        qWarning("thumbpad: warning: %s", qPrintable(w));
    for (const QString &e : errors)
        qWarning("thumbpad: error: %s", qPrintable(e));
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
            qWarning("thumbpad: no running instance: %s", qPrintable(r.error().message()));
            return 1;
        }
        return 0;
    }

    // Layout file: --layout, else the one chosen in settings, else the default.
    UserSettings userSettings;
    auto chooseLayout = [&parser, &layoutOpt, &userSettings] {
        if (parser.isSet(layoutOpt))
            return parser.value(layoutOpt);
        if (!userSettings.layout().isEmpty() && QFileInfo::exists(userSettings.layout()))
            return userSettings.layout();
        return findLayout(QString());
    };
    QString layoutPath = chooseLayout();
    std::shared_ptr<Layout> layout = loadLayout(layoutPath);
    if (!layout) {
        qWarning("thumbpad: cannot load layout %s", qPrintable(layoutPath));
        return 1;
    }

    if (parser.isSet(checkOpt)) {
        printf("%s", qPrintable(layout->summary()));
        return 0;
    }
    if (parser.isSet(previewOpt)) {
        QString err;
        if (!renderPreview(*layout, parser.value(previewOpt), &err)) {
            qWarning("thumbpad: %s", qPrintable(err));
            return 1;
        }
        printf("wrote %s\n", qPrintable(parser.value(previewOpt)));
        return 0;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kDBusService)) {
        qWarning("thumbpad: already running (use --toggle)");
        return 1;
    }

    UInput uinput;
    QString uerr;
    if (!uinput.open(&uerr))
        qWarning("thumbpad: warning: %s; Ctrl/Alt/Esc/arrows won't work", qPrintable(uerr));

    InputMethod *im = nullptr;
    if (QGuiApplication::platformName().startsWith("wayland")) {
        im = new InputMethod();
        QObject::connect(im, &QWaylandClientExtension::activeChanged, im, [im] {
            qWarning("thumbpad: input method protocol %s",
                    im->isActive() ? "bound" : "lost");
        });
        QTimer::singleShot(1500, im, [im] {
            if (!im->isActive())
                qWarning("thumbpad: input method protocol unavailable (not launched by "
                         "KWin?), text falls back to ASCII via uinput");
        });
    }

    Controller controller(im, &uinput);
    KeyboardUi ui(&controller);
    userSettings.setBase(layout->settings);
    ui.setSettings(userSettings.effective());
    ui.setLayout(layout);
    QObject::connect(&userSettings, &UserSettings::changed, &ui,
                     [&ui, &userSettings] { ui.setSettings(userSettings.effective()); });

    QQuickStyle::setStyle(QStringLiteral("Material"));
    SettingsWindow settingsWindow(&userSettings, &ui);
    QObject::connect(&ui, &KeyboardUi::settingsRequested, &settingsWindow, &SettingsWindow::open);
    if (im)
        QObject::connect(im, &InputMethod::contextChanged, &ui, &KeyboardUi::setImActive);

    auto reload = [&ui, &userSettings, &layoutPath] {
        if (auto l = loadLayout(layoutPath)) {
            userSettings.setBase(l->settings); // re-applies the effective settings
            ui.setLayout(l);
            qWarning("thumbpad: reloaded %s", qPrintable(layoutPath));
        }
    };
    DBusIface dbus(&controller, &ui, reload, &app);
    QObject::connect(&dbus, &DBusIface::settingsRequested, &settingsWindow, &SettingsWindow::open);
    bus.registerObject(kDBusPath, &dbus, QDBusConnection::ExportAllSlots);

    // Live reload while editing the layout. Editors often replace the file,
    // which drops the watch, so re-add it each time.
    QFileSystemWatcher watcher({layoutPath});
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(200);
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &debounce,
                     qOverload<>(&QTimer::start));
    QObject::connect(&debounce, &QTimer::timeout, &app, [&watcher, &layoutPath, reload] {
        if (!watcher.files().contains(layoutPath))
            watcher.addPath(layoutPath);
        reload();
    });
    // Another layout picked in the settings window.
    QObject::connect(&userSettings, &UserSettings::layoutChosen, &app,
                     [&watcher, &layoutPath, &chooseLayout, reload] {
        const QString next = chooseLayout();
        if (next == layoutPath)
            return;
        if (!watcher.files().isEmpty())
            watcher.removePaths(watcher.files());
        layoutPath = next;
        watcher.addPath(layoutPath);
        reload();
    });

    if (parser.isSet(showOpt))
        ui.show();
    qWarning("thumbpad: running with layout %s", qPrintable(layoutPath));
    const int rc = app.exec();
    delete im;
    return rc;
}
