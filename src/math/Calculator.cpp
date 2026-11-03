#include "math/Calculator.h"

#include "math/Expression.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <cmath>

namespace cb::math {

namespace {

QString tr(const char* s) { return QCoreApplication::translate("Calculator", s); }

QString replaceAns(const QString& expression, const QString& replacement)
{
    static const QRegularExpression re(QStringLiteral("\\bans\\b"), QRegularExpression::CaseInsensitiveOption);
    QString s = expression;
    s.replace(re, replacement);
    return s;
}

QString superscript(const QString& digits)
{
    static const QString sup = QStringLiteral("⁰¹²³⁴⁵⁶⁷⁸⁹");
    QString out;
    for (const QChar c : digits) {
        if (c.isDigit())
            out += sup[c.digitValue()];
        else if (c == QLatin1Char('-'))
            out += QChar(0x207B); // ⁻
    }
    return out;
}

// ------------------------------------------------------------------------------- LaTeX conversion

int matchingParen(const QString& s, int open)
{
    int depth = 0;
    for (int i = open; i < s.size(); ++i) {
        if (s[i] == QLatin1Char('('))
            ++depth;
        else if (s[i] == QLatin1Char(')') && --depth == 0)
            return i;
    }
    return -1;
}

QString latexOf(const QString& s);

// The operand that follows a √ or ^ at position i: "(…)", a number or a name. Returns its
// LaTeX and moves i past it.
QString operandAt(const QString& s, int& i)
{
    while (i < s.size() && s[i].isSpace())
        ++i;
    if (i >= s.size())
        return QString();
    if (s[i] == QLatin1Char('(')) {
        const int close = matchingParen(s, i);
        const int end = close < 0 ? s.size() : close;
        const QString inner = s.mid(i + 1, end - i - 1);
        i = close < 0 ? s.size() : close + 1;
        return latexOf(inner);
    }
    const int start = i;
    if (s[i].isDigit() || s[i] == QLatin1Char('.')) {
        while (i < s.size() && (s[i].isDigit() || s[i] == QLatin1Char('.')))
            ++i;
    } else if (s[i] == QChar(0x03C0)) {
        ++i;
    } else if (s[i].isLetter()) {
        while (i < s.size() && s[i].isLetter() && s[i] != QChar(0x03C0))
            ++i;
    } else {
        ++i;
    }
    return latexOf(s.mid(start, i - start));
}

QString latexOf(const QString& s)
{
    static const QStringList functions = {QStringLiteral("asin"), QStringLiteral("acos"), QStringLiteral("atan"),
                                          QStringLiteral("arcsin"), QStringLiteral("arccos"), QStringLiteral("arctan"),
                                          QStringLiteral("sinh"), QStringLiteral("cosh"), QStringLiteral("tanh"),
                                          QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
                                          QStringLiteral("ln"), QStringLiteral("log"), QStringLiteral("exp")};
    QString out;
    int i = 0;
    while (i < s.size()) {
        const QChar c = s[i];
        if (c.isSpace()) {
            ++i;
        } else if (c.isDigit() || c == QLatin1Char('.')) {
            const int start = i;
            while (i < s.size() && (s[i].isDigit() || s[i] == QLatin1Char('.')))
                ++i;
            // 1.5E+15 written by the calculator itself
            if (i + 1 < s.size() && s[i] == QLatin1Char('E')) {
                int j = i + 1;
                if (j < s.size() && (s[j] == QLatin1Char('+') || s[j] == QLatin1Char('-')))
                    ++j;
                const int expStart = j;
                while (j < s.size() && s[j].isDigit())
                    ++j;
                if (j > expStart) {
                    QString exponent = s.mid(i + 1, j - i - 1);
                    exponent.remove(QLatin1Char('+'));
                    out += s.mid(start, i - start) + QStringLiteral(" \\times 10^{") + exponent + QLatin1Char('}');
                    i = j;
                    continue;
                }
            }
            out += s.mid(start, i - start);
        } else if (c == QChar(0x03C0)) { // π (a letter for Qt, so before the names below)
            out += QStringLiteral("\\pi ");
            ++i;
        } else if (c.isLetter()) {
            const int start = i;
            while (i < s.size() && s[i].isLetter() && s[i] != QChar(0x03C0))
                ++i;
            QString word = s.mid(start, i - start);
            const QString lower = word.toLower();
            if (lower == QLatin1String("sqrt")) {
                out += QStringLiteral("\\sqrt{") + operandAt(s, i) + QLatin1Char('}');
            } else if (lower == QLatin1String("cbrt")) {
                out += QStringLiteral("\\sqrt[3]{") + operandAt(s, i) + QLatin1Char('}');
            } else if (lower == QLatin1String("pi")) {
                out += QStringLiteral("\\pi ");
            } else {
                // Split runs such as "sinx" the way the evaluator does: a known function first.
                bool matched = false;
                for (const QString& f : functions) {
                    if (lower.startsWith(f)) {
                        const QString name = f.startsWith(QLatin1Char('a')) && !f.startsWith(QLatin1String("arc"))
                            ? QStringLiteral("arc") + f.mid(1)
                            : f;
                        out += QLatin1Char('\\') + name + QLatin1Char(' ');
                        i = start + f.size();
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                    out += word;
            }
        } else if (c == QChar(0x221A)) { // √
            ++i;
            out += QStringLiteral("\\sqrt{") + operandAt(s, i) + QLatin1Char('}');
        } else if (c == QLatin1Char('^')) {
            ++i;
            out += QStringLiteral("^{") + operandAt(s, i) + QLatin1Char('}');
        } else if (c == QChar(0x00B2) || c == QChar(0x00B3)) {
            out += c == QChar(0x00B2) ? QStringLiteral("^{2}") : QStringLiteral("^{3}");
            ++i;
        } else if (c == QLatin1Char('*') || c == QChar(0x00D7) || c == QChar(0x00B7)) {
            out += QStringLiteral(" \\times ");
            ++i;
        } else if (c == QLatin1Char('/') || c == QChar(0x00F7)) {
            out += QStringLiteral(" \\div ");
            ++i;
        } else if (c == QLatin1Char('-') || c == QChar(0x2212)) {
            out += QStringLiteral(" - ");
            ++i;
        } else if (c == QLatin1Char('+')) {
            out += QStringLiteral(" + ");
            ++i;
        } else if (c == QLatin1Char('%')) {
            out += QStringLiteral("\\%");
            ++i;
        } else if (c == QLatin1Char('(')) {
            const int close = matchingParen(s, i);
            const int end = close < 0 ? s.size() : close;
            out += QStringLiteral("\\left(") + latexOf(s.mid(i + 1, end - i - 1)) + QStringLiteral("\\right)");
            i = close < 0 ? s.size() : close + 1;
        } else if (c == QLatin1Char(')')) {
            ++i; // unmatched: drop
        } else {
            out += c;
            ++i;
        }
    }
    return out.simplified();
}

} // namespace

Calculator::Result Calculator::evaluate(const QString& expression) const
{
    Result r;
    QString text = expression.trimmed();
    while (text.endsWith(QLatin1Char('=')))
        text.chop(1);
    text = text.trimmed();
    if (text.isEmpty()) {
        r.error = tr("Type a calculation");
        return r;
    }
    // "2××3" is a slip, not a power (the shared engine reads ** as ^ for typed formulas).
    static const QRegularExpression doubled(QStringLiteral("[×÷*/·]\\s*[×÷*/·]"));
    if (text.contains(doubled)) {
        r.error = tr("Two operators in a row");
        return r;
    }
    QString input = replaceAns(text, QLatin1Char('(') + toInput(m_ans) + QLatin1Char(')'));
    // Like a school calculator, brackets left open at the end are closed: "sin(30" = sin(30).
    const int open = input.count(QLatin1Char('(')) - input.count(QLatin1Char(')'));
    if (open > 0)
        input += QString(open, QLatin1Char(')'));
    Expression::Options options;
    options.degrees = m_unit == AngleUnit::Degrees;
    QString error;
    const Expression e = Expression::compile(input, &error, options);
    if (!e.isValid()) {
        r.error = error;
        return r;
    }
    if (!e.variables().isEmpty()) {
        r.error = tr("“%1” is not a number or a function the calculator knows").arg(e.variables().first());
        return r;
    }
    const double v = e.evaluate(nullptr, 0);
    if (std::isnan(v)) {
        r.error = tr("Not defined (for example √ of a negative number, 0 ÷ 0 or tan 90°)");
        return r;
    }
    if (std::isinf(v)) {
        r.error = tr("Too large, or division by zero");
        return r;
    }
    r.ok = true;
    r.value = v == 0.0 ? 0.0 : v; // no "-0"
    r.text = format(r.value);
    return r;
}

Calculator::Result Calculator::calculate(const QString& expression)
{
    const Result r = evaluate(expression);
    if (!r.ok)
        return r;
    QString shown = expression.trimmed();
    while (shown.endsWith(QLatin1Char('=')))
        shown.chop(1);
    m_history.prepend({substituteAns(shown.trimmed(), m_ans), r.text, r.value});
    while (m_history.size() > kMaxHistory)
        m_history.removeLast();
    m_ans = r.value;
    return r;
}

QString Calculator::format(double value)
{
    if (value == 0.0 || std::isnan(value))
        return QStringLiteral("0");
    QString s;
    if (std::abs(value) < 1e15 && value == std::floor(value))
        s = QString::number(static_cast<qint64>(value));
    else
        s = QString::number(value, 'g', 12);
    const int e = s.indexOf(QLatin1Char('e'));
    if (e >= 0) {
        QString exponent = s.mid(e + 1);
        exponent.remove(QLatin1Char('+'));
        while (exponent.startsWith(QLatin1Char('0')) || exponent.startsWith(QLatin1String("-0")))
            exponent.remove(exponent.startsWith(QLatin1Char('-')) ? 1 : 0, 1);
        s = s.left(e) + QStringLiteral(" × 10") + superscript(exponent);
    }
    s.replace(QLatin1Char('-'), QChar(0x2212));
    return s;
}

QString Calculator::toInput(double value)
{
    QString s = QString::number(value, 'g', 15);
    s.replace(QLatin1Char('e'), QLatin1Char('E'));
    return s;
}

QString Calculator::substituteAns(const QString& expression, double ans)
{
    const QString shown = format(ans);
    return replaceAns(expression, ans < 0 ? QLatin1Char('(') + shown + QLatin1Char(')') : shown);
}

QString Calculator::toLatex(const QString& expression, const QString& result)
{
    QString value = result;
    value.replace(QChar(0x2212), QLatin1Char('-'));
    const int times = value.indexOf(QStringLiteral(" × 10"));
    if (times >= 0) {
        QString exponent;
        for (const QChar c : value.mid(times + 5)) {
            const int d = QStringLiteral("⁰¹²³⁴⁵⁶⁷⁸⁹").indexOf(c);
            exponent += d >= 0 ? QString::number(d) : (c == QChar(0x207B) ? QStringLiteral("-") : QString());
        }
        value = value.left(times) + QStringLiteral(" \\times 10^{") + exponent + QLatin1Char('}');
    }
    return latexOf(expression) + QStringLiteral(" = ") + value;
}

} // namespace cb::math
