#pragma once

#include <QObject>
#include <QPointer>
#include <memory>

class QQmlApplicationEngine;
class QQuickWindow;

namespace tp {

class KeyboardUi;
class UserSettings;

// Owns the QML settings panel. Created lazily on first open, so the keyboard
// itself doesn't pay for Qt Quick at startup.
class SettingsWindow : public QObject
{
    Q_OBJECT
public:
    SettingsWindow(UserSettings *settings, KeyboardUi *ui, QObject *parent = nullptr);
    ~SettingsWindow() override;

    void open();

private:
    void place();

    UserSettings *m_settings;
    KeyboardUi *m_ui;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_window;
};

} // namespace tp
