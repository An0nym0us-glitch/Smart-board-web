#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace cb::math {

/// A compiled real-valued expression such as "a*sin(bx + c)" or "2x² − 3x + 1".
///
/// Supported: + − * / ^, unary minus, implicit multiplication (2x, ax, 3(x+1), (x+1)(x−1), 2sin x),
/// |x|, functions (sin cos tan cot sec csc asin acos atan sinh cosh tanh sqrt cbrt abs ln log exp
/// floor ceil round sign min max), constants (pi, π, e), Unicode operators (× · ÷ − ² ³ √),
/// postfix factorial (5!) and percent (15% = 0.15). Angles are radians unless compiled with
/// Options::degrees (the calculator's DEG mode), where sin 30 = 0.5 and asin 0.5 = 30.
/// Identifiers made of several unknown letters are split into single-letter variables ("abx"
/// is a·b·x) which matches how teachers write parameters.
///
/// Compilation produces a small stack program so graphs can evaluate it millions of times.
class Expression
{
public:
    Expression();

    struct Options
    {
        bool degrees = false; ///< trigonometric functions take and return degrees
    };

    /// Parses and compiles. On failure the returned expression is invalid and error is set.
    static Expression compile(const QString& text, QString* error = nullptr);
    static Expression compile(const QString& text, QString* error, const Options& options);

    bool isValid() const { return m_program != nullptr; }
    const QString& source() const { return m_source; }

    /// Variables in slot order (e.g. {"x", "a", "b"}).
    const QStringList& variables() const { return m_variables; }
    int slotOf(const QString& name) const { return m_variables.indexOf(name); }
    bool usesVariable(const QString& name) const { return m_variables.contains(name); }

    /// Evaluates with values given per slot. Missing values count as 0.
    double evaluate(const double* values, int count) const;
    double evaluate(const QHash<QString, double>& values) const;

    /// Splits "f(x) = expr" / "y = expr" into name and body. Returns the body.
    static QString stripDefinition(const QString& text, QString* name = nullptr);

    struct Program;

private:
    std::shared_ptr<const Program> m_program;
    QString m_source;
    QStringList m_variables;
};

} // namespace cb::math
