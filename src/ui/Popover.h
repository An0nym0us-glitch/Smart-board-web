#pragma once

#include <QPainterPath>
#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QScrollArea;
class QVariantAnimation;
class QVBoxLayout;

namespace cb {

struct UiContext;
class TouchButton;

/// ClassBoard's single popover component.
///
/// A popover floats above the ribbon inside the main window (it is a child widget, never a
/// separate OS window), has rounded corners, a soft shadow and a notch pointing at the control
/// that opened it. Every contextual panel (MORE, Pen, Shapes, Geometry, Pages ...) is a Popover,
/// so they all share animation, shape, typography, spacing, touch scrolling and positioning.
class Popover : public QWidget
{
    Q_OBJECT
public:
    Popover(const UiContext& ui, const QString& key, const QString& title, QWidget* parent = nullptr);

    QString key() const { return m_key; }
    void setTitle(const QString& title);
    /// Sets the body widget (takes ownership). Tall content scrolls with touch kinetic scrolling.
    void setContent(QWidget* content);
    QWidget* content() const { return m_content; }
    void setBackVisible(bool visible);
    void setMinimumContentWidth(int dp) { m_minContentWidthDp = dp; }

    /// Computes geometry within bounds (host coordinates) for an anchor rect (host coordinates).
    void place(const QRect& anchor, const QRect& bounds);

    void animateIn();
    void animateOut(std::function<void()> done);

signals:
    void backRequested();
    void closeRequested();
    /// The content's size hint changed (widgets shown / hidden): the host re-places the popover.
    void contentResized();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPainterPath bubblePath() const;
    void updateMargins();

    const UiContext& m_ui;
    QString m_key;
    QLabel* m_title = nullptr;
    TouchButton* m_back = nullptr;
    TouchButton* m_close = nullptr;
    QScrollArea* m_scroll = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_content = nullptr;
    QVariantAnimation* m_anim = nullptr;
    int m_minContentWidthDp = 300;
    bool m_notchBelow = true;
    qreal m_notchX = 0.0;
    QPoint m_targetPos;
    QSize m_lastHint;
    bool m_resizePending = false;
};

/// Transparent overlay covering the canvas area that owns the open popovers, closes them when
/// the user taps outside and keeps them positioned against their anchors.
class PopoverHost : public QWidget
{
    Q_OBJECT
public:
    PopoverHost(const UiContext& ui, QWidget* parent);

    /// Opens a popover anchored to a widget (e.g. a ribbon button). Closes any open popover.
    void open(Popover* popover, QWidget* anchor);
    /// Opens a popover anchored to a rect in host coordinates (e.g. an object on the canvas).
    void openAt(Popover* popover, const QRect& anchorRect);
    /// Replaces the current popover by a child popover at the same anchor (with a back button).
    void push(Popover* popover);
    void back();
    void closeAll();
    /// Swaps the top popover for a rebuilt one without animation.
    void replaceTop(Popover* popover);

    Popover* current() const { return m_stack.isEmpty() ? nullptr : m_stack.last().data(); }
    QString currentKey() const { return current() ? current()->key() : QString(); }
    bool isOpen() const { return current() != nullptr; }
    QWidget* anchorWidget() const { return m_anchor.data(); }

    void reposition();

signals:
    void opened(const QString& key);
    void closed(const QString& key);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QRect anchorRect() const;
    void show(Popover* popover, bool animate);
    void updateInteractive();

    const UiContext& m_ui;
    QVector<QPointer<Popover>> m_stack;
    QPointer<QWidget> m_anchor;
    QRect m_anchorRect;
};

} // namespace cb
