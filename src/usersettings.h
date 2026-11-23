#pragma once

#include "layout.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace tp {

// User preferences in ~/.config/thumbpad/settings.ini. Values set here
// override the "settings" block of the layout file, so regenerating a layout
// never wipes them. Exposed to the settings window (QML) through get()/set():
// bindings depend on `revision`, which changes whenever any value does.
class UserSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QStringList layouts READ layouts NOTIFY changed)
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY changed)

public:
    explicit UserSettings(QObject *parent = nullptr);

    // The layout's own settings, before user overrides.
    void setBase(const Settings &base);
    Settings effective() const;

    Q_INVOKABLE QVariant get(const QString &key) const;
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE bool isOverridden(const QString &key) const { return m_overrides.contains(key); }
    Q_INVOKABLE void resetDefaults();

    int revision() const { return m_revision; }
    QStringList layouts() const;
    QString layout() const { return m_layout; }  // chosen layout file, "" = automatic
    void setLayout(const QString &path);

Q_SIGNALS:
    void changed();
    void layoutChosen(const QString &path);

private:
    void save() const;

    Settings m_base;
    QVariantMap m_overrides;
    QString m_layout;
    int m_revision = 0;
};

} // namespace tp
