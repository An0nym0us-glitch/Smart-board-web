#pragma once

#include <QString>
#include <QVector>

namespace cb::math {

/// The classroom calculator behind the Calculator panel: evaluates what is typed or tapped
/// ("12×7", "√(2)+3²", "sin 30", "5!", "15% × 80", "Ans ÷ 4") with the same expression
/// engine as graphs, keeps the last answer (Ans) and a short history, and formats results the
/// way a school calculator shows them. It never changes the lesson by itself.
class Calculator
{
public:
    enum class AngleUnit { Degrees, Radians };

    struct Result
    {
        bool ok = false;
        double value = 0.0;
        QString text;  ///< formatted result, e.g. "84", "0.333333333333", "1.5 × 10¹⁵"
        QString error; ///< why there is no result (ok == false)
    };

    struct Entry
    {
        QString expression; ///< as typed, Ans replaced by its value
        QString result;     ///< formatted
        double value = 0.0;
    };

    static constexpr int kMaxHistory = 20;

    void setAngleUnit(AngleUnit unit) { m_unit = unit; }
    AngleUnit angleUnit() const { return m_unit; }

    /// Evaluates without changing anything (live preview while typing).
    Result evaluate(const QString& expression) const;
    /// Evaluates and, if it works, remembers the result as Ans and in the history.
    Result calculate(const QString& expression);

    /// The calculation being typed, kept while the calculator panel is closed.
    const QString& input() const { return m_input; }
    void setInput(const QString& input) { m_input = input; }
    /// True right after "=": the next operator continues with Ans, a number starts afresh.
    bool showingResult() const { return m_showingResult; }
    void setShowingResult(bool on) { m_showingResult = on; }

    double ans() const { return m_ans; }
    const QVector<Entry>& history() const { return m_history; }
    void clearHistory() { m_history.clear(); }

    /// "84", "-2.5" (typographic minus), "0.333333333333", "1.5 × 10¹⁵": 12 significant digits,
    /// whole numbers below 10¹⁵ in full.
    static QString format(double value);
    /// The value as text the calculator reads back at full precision ("1.5E+15").
    static QString toInput(double value);
    /// The expression with Ans replaced by the given value (shown in the history and on the board).
    static QString substituteAns(const QString& expression, double ans);
    /// LaTeX for the equation that "Put on board" inserts, e.g. "12 \times 7 = 84".
    static QString toLatex(const QString& expression, const QString& result);

private:
    AngleUnit m_unit = AngleUnit::Degrees;
    double m_ans = 0.0;
    QVector<Entry> m_history;
    QString m_input;
    bool m_showingResult = false;
};

} // namespace cb::math
