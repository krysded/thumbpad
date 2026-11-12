#include "dbusiface.h"
#include "controller.h"
#include "keyboardui.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace tp {

DBusIface::DBusIface(Controller *c, KeyboardUi *ui, std::function<void()> reload, QObject *parent)
    : QObject(parent), m_controller(c), m_ui(ui), m_reload(std::move(reload))
{
}

void DBusIface::Toggle() { m_ui->toggle(); }
void DBusIface::Show() { m_ui->show(); }
void DBusIface::Hide() { m_ui->hide(); }
void DBusIface::Reload() { m_reload(); }
void DBusIface::ToggleReserve() { m_ui->setReserveSpace(!m_ui->reserveSpace()); }
void DBusIface::ResetPositions() { m_ui->resetPositions(); }
void DBusIface::MoveBlock(int index, double dx, double dy) { m_ui->moveBlock(index, QPointF(dx, dy)); }

QString DBusIface::Press(const QString &actionJson)
{
    // Wrap in an array so a bare JSON string ("\"a\"") parses too.
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(("[" + actionJson + "]").toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || doc.array().size() != 1)
        return QStringLiteral("error: expected one JSON action, e.g. \"\\\"a\\\"\" or {\"key\":\"esc\"}");
    QString err;
    const Action a = parseAction(doc.array().at(0), &err);
    if (!err.isEmpty())
        return "error: " + err;
    m_controller->execute(a);
    return QStringLiteral("ok");
}

QString DBusIface::State()
{
    return m_controller->stateString() +
           (m_ui->isShown() ? QStringLiteral(" visible") : QStringLiteral(" hidden")) +
           (m_ui->reserveSpace() ? QStringLiteral(" reserve") : QString());
}

} // namespace tp
