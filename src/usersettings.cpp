#include "usersettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace tp {

namespace {

QSettings settingsFile()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, "thumbpad", "settings");
}

// Every key the settings window can change. Must match Settings fields.
const QStringList kKeys{"keyMm", "gapMm", "opacity", "swipeThresholdMm", "slideStepMm",
                        "longPressMs", "repeatDelayMs", "repeatIntervalMs", "showHandle"};

} // namespace

UserSettings::UserSettings(QObject *parent) : QObject(parent)
{
    QSettings st = settingsFile();
    st.beginGroup("keyboard");
    for (const QString &k : kKeys)
        if (st.contains(k))
            m_overrides.insert(k, st.value(k));
    st.endGroup();
    m_layout = st.value("layout").toString();
}

void UserSettings::save() const
{
    QSettings st = settingsFile();
    st.remove("keyboard");
    st.beginGroup("keyboard");
    for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it)
        st.setValue(it.key(), it.value());
    st.endGroup();
    if (m_layout.isEmpty())
        st.remove("layout");
    else
        st.setValue("layout", m_layout);
}

void UserSettings::setBase(const Settings &base)
{
    m_base = base;
    ++m_revision;
    Q_EMIT changed();
}

Settings UserSettings::effective() const
{
    Settings s = m_base;
    auto d = [this](const char *k, double &field) {
        if (m_overrides.contains(k)) field = m_overrides.value(k).toDouble();
    };
    auto i = [this](const char *k, int &field) {
        if (m_overrides.contains(k)) field = m_overrides.value(k).toInt();
    };
    d("keyMm", s.keyMm);
    d("gapMm", s.gapMm);
    d("opacity", s.opacity);
    d("swipeThresholdMm", s.swipeThresholdMm);
    d("slideStepMm", s.slideStepMm);
    i("longPressMs", s.longPressMs);
    i("repeatDelayMs", s.repeatDelayMs);
    i("repeatIntervalMs", s.repeatIntervalMs);
    if (m_overrides.contains("showHandle"))
        s.showHandle = m_overrides.value("showHandle").toBool();
    return s;
}

QVariant UserSettings::get(const QString &key) const
{
    const Settings s = effective();
    if (key == "keyMm") return s.keyMm;
    if (key == "gapMm") return s.gapMm;
    if (key == "opacity") return s.opacity;
    if (key == "swipeThresholdMm") return s.swipeThresholdMm;
    if (key == "slideStepMm") return s.slideStepMm;
    if (key == "longPressMs") return s.longPressMs;
    if (key == "repeatDelayMs") return s.repeatDelayMs;
    if (key == "repeatIntervalMs") return s.repeatIntervalMs;
    if (key == "showHandle") return s.showHandle;
    qWarning("thumbpad: unknown setting \"%s\"", qPrintable(key));
    return {};
}

void UserSettings::set(const QString &key, const QVariant &value)
{
    if (!kKeys.contains(key)) {
        qWarning("thumbpad: unknown setting \"%s\"", qPrintable(key));
        return;
    }
    if (m_overrides.value(key) == value)
        return;
    m_overrides.insert(key, value);
    save();
    ++m_revision;
    Q_EMIT changed();
}

void UserSettings::resetDefaults()
{
    m_overrides.clear();
    save();
    ++m_revision;
    Q_EMIT changed();
}

QStringList UserSettings::layouts() const
{
    QStringList out;
    const QString cfg =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/thumbpad";
    if (QFileInfo::exists(cfg + "/layout.json"))
        out << cfg + "/layout.json";
    QStringList dirs{cfg + "/layouts"};
    dirs << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "thumbpad/layouts",
                                      QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs)
        for (const QFileInfo &fi : QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name))
            if (!out.contains(fi.absoluteFilePath()))
                out << fi.absoluteFilePath();
    return out;
}

void UserSettings::setLayout(const QString &path)
{
    if (path == m_layout)
        return;
    m_layout = path;
    save();
    ++m_revision;
    Q_EMIT changed();
    Q_EMIT layoutChosen(path);
}

} // namespace tp
