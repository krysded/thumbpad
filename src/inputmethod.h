#pragma once

#include "qwayland-input-method-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <memory>

namespace tp {

// One text field session handed to us by the compositor.
class InputMethodContext : public QtWayland::zwp_input_method_context_v1
{
public:
    explicit InputMethodContext(::zwp_input_method_context_v1 *obj)
        : QtWayland::zwp_input_method_context_v1(obj) {}
    ~InputMethodContext() override { destroy(); }

    uint32_t serial = 0;

protected:
    void zwp_input_method_context_v1_commit_state(uint32_t s) override { serial = s; }
};

// zwp_input_method_v1: KWin offers it only to the process it launched as the
// virtual keyboard (kwinrc [Wayland] InputMethod=...). Text is committed here,
// so any Unicode works regardless of the active xkb layout.
class InputMethod : public QWaylandClientExtensionTemplate<InputMethod>,
                    public QtWayland::zwp_input_method_v1
{
    Q_OBJECT
public:
    InputMethod();

    bool hasContext() const { return bool(m_ctx); }
    void commitText(const QString &text);

Q_SIGNALS:
    void contextChanged(bool active);

protected:
    void zwp_input_method_v1_activate(::zwp_input_method_context_v1 *id) override;
    void zwp_input_method_v1_deactivate(::zwp_input_method_context_v1 *context) override;

private:
    std::unique_ptr<InputMethodContext> m_ctx;
};

} // namespace tp
