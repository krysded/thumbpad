#pragma once

#include "layout.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <memory>

namespace tp {

class BlockWindow;
class Controller;

// Owns the block windows: picks blocks for the current layer and orientation,
// places them with layer-shell, and decides when they are visible.
class KeyboardUi : public QObject
{
    Q_OBJECT
public:
    explicit KeyboardUi(Controller *controller, QObject *parent = nullptr);
    ~KeyboardUi() override;

    void setLayout(std::shared_ptr<const Layout> layout);
    const Layout *layout() const { return m_layout.get(); }

    void show();   // pin visible
    void hide();   // hide until the next text field activation
    void toggle();
    bool isShown() const { return m_visible; }
    void setImActive(bool active);

private:
    void onStateChanged();
    void rebuild();
    void applyBlocks();
    void updateVisibility();
    bool portrait() const;
    const LayerDef &currentLayer() const;
    double pxPerMm() const;

    Controller *m_controller;
    std::shared_ptr<const Layout> m_layout;
    QList<QPointer<BlockWindow>> m_windows;
    QString m_shownLayer;
    bool m_shownPortrait = false;
    bool m_pinned = false;
    bool m_suppressed = false;
    bool m_imActive = false;
    bool m_visible = false;
};

} // namespace tp
