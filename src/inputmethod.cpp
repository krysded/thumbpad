#include "inputmethod.h"

namespace tp {

InputMethod::InputMethod()
    : QWaylandClientExtensionTemplate<InputMethod>(1)
{
    initialize();
}

void InputMethod::commitText(const QString &text)
{
    if (m_ctx)
        m_ctx->commit_string(m_ctx->serial, text);
}

void InputMethod::zwp_input_method_v1_activate(::zwp_input_method_context_v1 *id)
{
    m_ctx = std::make_unique<InputMethodContext>(id);
    Q_EMIT contextChanged(true);
}

void InputMethod::zwp_input_method_v1_deactivate(::zwp_input_method_context_v1 *context)
{
    if (m_ctx && m_ctx->object() == context) {
        m_ctx.reset();
        Q_EMIT contextChanged(false);
    }
}

} // namespace tp
