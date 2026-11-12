#pragma once

#include <QList>
#include <QString>

namespace tp {

// A virtual evdev keyboard. To the compositor it looks like real hardware, so
// chords such as Ctrl+A reach every application, terminals and Emacs included.
class UInput
{
public:
    UInput() = default;
    ~UInput();
    UInput(const UInput &) = delete;
    UInput &operator=(const UInput &) = delete;

    bool open(QString *error);
    bool isOpen() const { return m_fd >= 0; }

    // Presses keys in order, then releases them in reverse order.
    void chord(const QList<int> &keys);
    // Types one ASCII character using the US layout. False if unmappable.
    bool typeChar(QChar c);

private:
    void write(int type, int code, int value);
    int m_fd = -1;
};

} // namespace tp
