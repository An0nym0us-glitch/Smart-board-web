#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

namespace cb {

struct AppServices;
class TouchButton;

/// The calculator popover (MORE → Calculator): big touch keys for arithmetic, powers, roots,
/// trigonometry (DEG / RAD), logarithms, factorial, percent and Ans; a live result while typing;
/// the recent calculations; and "Put on board", which writes the calculation and its result
/// onto the page as an editable equation (one undo step). Typing on a keyboard works too.
class CalculatorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CalculatorPanel(const AppServices& services, QWidget* parent = nullptr);

    /// The key labels, row by row (also used by tests).
    static const QVector<QStringList>& keys();
    /// Presses a key as if it was tapped ("7", "+", "sin", "=", "AC", "⌫" ...).
    void press(const QString& key);
    QString input() const;
    QString resultText() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void insert(const QString& text);
    void backspace();
    void equals();
    void clearAll();
    void putOnBoard();
    void setInput(const QString& text, int cursor = -1);
    void updatePreview();
    void rebuildHistory();
    void setJustEvaluated(bool on);
    void showResult(const QString& text);

    const AppServices& m_s;
    QLineEdit* m_input = nullptr;
    QLabel* m_result = nullptr;
    QWidget* m_history = nullptr;
    TouchButton* m_deg = nullptr;
    TouchButton* m_rad = nullptr;
    bool m_justEvaluated = false;
};

} // namespace cb
