#pragma once

#include "layout.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRasterWindow>
#include <memory>

namespace tp {

class BlockWindow;
class Controller;

// Invisible full-width strip whose only job is a layer-shell exclusive zone:
// it makes KWin keep the area above the keyboard free (maximized windows shrink).
// The blocks can't do this themselves because corner-anchored surfaces can't
// reserve space.
class SpacerWindow : public QRasterWindow
{
public:
    SpacerWindow();

protected:
    void paintEvent(QPaintEvent *) override;
};

// Small tab at the right screen edge, shown while the keyboard is hidden, so
// it can always be brought back without a hardware keyboard.
class HandleWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HandleWindow();

Q_SIGNALS:
    void tapped();

protected:
    bool event(QEvent *e) override;
    void paintEvent(QPaintEvent *) override;
};

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

    void setReserveSpace(bool reserve);
    bool reserveSpace() const { return m_reserve; }
    void moveBlock(int index, QPointF delta);
    void resetPositions();
    void saveState() const;

private:
    void onStateChanged();
    void onCommand(const QString &command);
    void rebuild();
    void applyBlocks();
    void updateVisibility();
    void placeAll();
    void createHandle();
    void updateSpacer();
    QString offsetKey() const;
    bool portrait() const;
    const LayerDef &currentLayer() const;
    double pxPerMm() const;

    Controller *m_controller;
    std::shared_ptr<const Layout> m_layout;
    QList<QPointer<BlockWindow>> m_windows;
    QPointer<SpacerWindow> m_spacer;
    QPointer<HandleWindow> m_handle;
    // Per orientation, shared by all blocks so a split keyboard moves as one:
    // x = towards the screen centre, y = up (logical px).
    QHash<QString, QPointF> m_offsets;
    QString m_shownLayer;
    bool m_shownPortrait = false;
    bool m_pinned = false;
    bool m_suppressed = false;
    bool m_imActive = false;
    bool m_visible = false;
    bool m_reserve = false;
};

} // namespace tp
