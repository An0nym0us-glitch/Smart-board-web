#pragma once

#include <QTimer>
#include <QVector>
#include <QWidget>

#include <functional>

class QHBoxLayout;
class QLabel;

namespace cb {

struct UiContext;
class TouchButton;

/// In-app notification banner shown at the top of the board (status, progress, quick actions).
/// It never takes focus and never blocks the canvas.
class Toast : public QWidget
{
    Q_OBJECT
public:
    struct Action
    {
        QString label;
        std::function<void()> callback;
        bool primary = false;
    };

    Toast(const UiContext& ui, QWidget* parent);

    void showMessage(const QString& text, int milliseconds = 3000);
    /// Progress 0..100; negative hides the bar. Stays until another message replaces it.
    void showProgress(const QString& text, int percent);
    void showActions(const QString& text, const QVector<Action>& actions, int milliseconds = 0);
    void dismiss();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void present(int milliseconds);
    void clearActions();
    void reposition();
    bool eventFilter(QObject* watched, QEvent* event) override;

    const UiContext& m_ui;
    QLabel* m_label = nullptr;
    QHBoxLayout* m_actions = nullptr;
    QVector<TouchButton*> m_buttons;
    int m_progress = -1;
    QTimer m_timer;
};

/// Modal, in-window confirmation card (dims the board) used instead of OS message boxes.
class ConfirmOverlay : public QWidget
{
    Q_OBJECT
public:
    struct Choice
    {
        QString label;
        std::function<void()> callback;
        bool primary = false;
        bool danger = false;
    };

    /// Shows the overlay over parent. The overlay deletes itself after a choice (or tapping the
    /// dimmed area, which runs no callback).
    static ConfirmOverlay* ask(const UiContext& ui, QWidget* parent, const QString& title, const QString& message,
                               const QVector<Choice>& choices);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ConfirmOverlay(const UiContext& ui, QWidget* parent);
    const UiContext& m_ui;
    QWidget* m_card = nullptr;
};

} // namespace cb
