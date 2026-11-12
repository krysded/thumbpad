#include "uinput.h"
#include "actions.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace tp {

UInput::~UInput()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        ::close(m_fd);
    }
}

bool UInput::open(QString *error)
{
    m_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        *error = QStringLiteral("cannot open /dev/uinput: %1 (see udev/README.md)")
                     .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(m_fd, UI_SET_EVBIT, EV_SYN);
    for (int k = KEY_ESC; k <= KEY_MICMUTE; ++k)
        ioctl(m_fd, UI_SET_KEYBIT, k);

    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x7470; // "tp"
    setup.id.product = 0x0001;
    std::strncpy(setup.name, "thumbpad virtual keyboard", UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(m_fd, UI_DEV_SETUP, &setup) < 0 || ioctl(m_fd, UI_DEV_CREATE) < 0) {
        *error = QStringLiteral("uinput device setup failed: %1")
                     .arg(QString::fromLocal8Bit(strerror(errno)));
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    return true;
}

void UInput::write(int type, int code, int value)
{
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (::write(m_fd, &ev, sizeof ev) != sizeof ev)
        qWarning("thumbpad: uinput write failed: %s", strerror(errno));
}

void UInput::chord(const QList<int> &keys)
{
    if (m_fd < 0 || keys.isEmpty())
        return;
    for (int k : keys) {
        write(EV_KEY, k, 1);
        write(EV_SYN, SYN_REPORT, 0);
    }
    for (auto it = keys.crbegin(); it != keys.crend(); ++it) {
        write(EV_KEY, *it, 0);
        write(EV_SYN, SYN_REPORT, 0);
    }
}

bool UInput::typeChar(QChar c)
{
    int code;
    bool shift;
    if (!asciiToKey(c, &code, &shift))
        return false;
    if (shift)
        chord({KEY_LEFTSHIFT, code});
    else
        chord({code});
    return true;
}

} // namespace tp
