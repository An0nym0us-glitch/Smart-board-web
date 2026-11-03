// The classroom calculator: evaluation, angle modes, factorial / percent, Ans and history,
// result formatting and the equation that "Put on board" inserts.
#include "math/Calculator.h"
#include "math/Expression.h"
#include "math/equation/MathLayout.h"

#include <QtTest>

#include <cmath>

using namespace cb::math;

namespace {
double value(Calculator& c, const QString& expression)
{
    const Calculator::Result r = c.evaluate(expression);
    if (!r.ok)
        qWarning() << expression << "->" << r.error;
    return r.ok ? r.value : std::nan("");
}

QString shown(Calculator& c, const QString& expression) { return c.evaluate(expression).text; }
} // namespace

class TestCalculator : public QObject
{
    Q_OBJECT
private slots:
    void arithmetic()
    {
        Calculator c;
        QCOMPARE(value(c, QStringLiteral("12×7")), 84.0);
        QCOMPARE(value(c, QStringLiteral("2+3×4")), 14.0);         // × before +
        QCOMPARE(value(c, QStringLiteral("(2+3)×4")), 20.0);
        QCOMPARE(value(c, QStringLiteral("7÷2")), 3.5);
        QCOMPARE(value(c, QStringLiteral("−5+2")), -3.0);          // typographic minus
        QCOMPARE(value(c, QStringLiteral("2^10")), 1024.0);
        QCOMPARE(value(c, QStringLiteral("3²+4²")), 25.0);
        QCOMPARE(value(c, QStringLiteral("√(9)+√16")), 7.0);
        QCOMPARE(value(c, QStringLiteral("2(3+4)")), 14.0);        // implicit multiplication
        QCOMPARE(value(c, QStringLiteral("12×7 =")), 84.0);        // trailing "=" is fine
        QCOMPARE(value(c, QStringLiteral("2×(3+4")), 14.0);        // open bracket closed at the end
        QCOMPARE(value(c, QStringLiteral("√(16")), 4.0);
        QCOMPARE(shown(c, QStringLiteral("0.1+0.2")), QStringLiteral("0.3"));
        QCOMPARE(shown(c, QStringLiteral("1÷3")), QStringLiteral("0.333333333333"));
        QCOMPARE(shown(c, QStringLiteral("2÷3")), QStringLiteral("0.666666666667"));
    }

    void degreesAreExactForSchoolAngles()
    {
        Calculator c; // degrees by default, like a school calculator
        QCOMPARE(c.angleUnit(), Calculator::AngleUnit::Degrees);
        QCOMPARE(shown(c, QStringLiteral("sin 30")), QStringLiteral("0.5"));
        QCOMPARE(shown(c, QStringLiteral("sin(90)")), QStringLiteral("1"));
        QCOMPARE(shown(c, QStringLiteral("cos 60")), QStringLiteral("0.5"));
        QCOMPARE(shown(c, QStringLiteral("cos 90")), QStringLiteral("0"));
        QCOMPARE(shown(c, QStringLiteral("tan 45")), QStringLiteral("1"));
        QCOMPARE(shown(c, QStringLiteral("sin 180")), QStringLiteral("0"));
        QCOMPARE(shown(c, QStringLiteral("sin(−30)")), QStringLiteral("−0.5"));
        QCOMPARE(shown(c, QStringLiteral("asin 0.5")), QStringLiteral("30"));
        QCOMPARE(shown(c, QStringLiteral("atan 1")), QStringLiteral("45"));
        QCOMPARE(shown(c, QStringLiteral("sin 45")), QStringLiteral("0.707106781187"));
        QVERIFY(!c.evaluate(QStringLiteral("tan 90")).ok); // undefined, not 1.6 × 10¹⁶
        c.setAngleUnit(Calculator::AngleUnit::Radians);
        QCOMPARE(shown(c, QStringLiteral("sin(π÷2)")), QStringLiteral("1"));
        QCOMPARE(shown(c, QStringLiteral("cos(π)")), QStringLiteral("−1"));
    }

    void factorialPercentAndConstants()
    {
        Calculator c;
        QCOMPARE(value(c, QStringLiteral("5!")), 120.0);
        QCOMPARE(value(c, QStringLiteral("0!")), 1.0);
        QCOMPARE(value(c, QStringLiteral("3!^2")), 36.0);
        QCOMPARE(value(c, QStringLiteral("2^3!")), 64.0);
        QVERIFY(!c.evaluate(QStringLiteral("2.5!")).ok);
        QVERIFY(!c.evaluate(QStringLiteral("(−1)!")).ok);
        QCOMPARE(value(c, QStringLiteral("15%×80")), 12.0);
        QCOMPARE(value(c, QStringLiteral("50%")), 0.5);
        QCOMPARE(shown(c, QStringLiteral("π")), QStringLiteral("3.14159265359"));
        QCOMPARE(shown(c, QStringLiteral("2π")), QStringLiteral("6.28318530718"));
        QCOMPARE(shown(c, QStringLiteral("e")), QStringLiteral("2.71828182846"));
        QCOMPARE(value(c, QStringLiteral("log 1000")), 3.0);
        QCOMPARE(value(c, QStringLiteral("ln(e^2)")), 2.0);
    }

    void graphsAreUnchanged()
    {
        // Graphs still work in radians and know no degree mode.
        const Expression e = Expression::compile(QStringLiteral("sin(x)"));
        QVERIFY(e.isValid());
        const double x = 3.14159265358979323846 / 2;
        QVERIFY(std::abs(e.evaluate(&x, 1) - 1.0) < 1e-12);
    }

    void errorsAreExplained()
    {
        Calculator c;
        QCOMPARE(c.evaluate(QString()).ok, false);
        QVERIFY(!c.evaluate(QStringLiteral("   ")).error.isEmpty());
        const Calculator::Result divZero = c.evaluate(QStringLiteral("1÷0"));
        QVERIFY(!divZero.ok);
        QVERIFY(divZero.error.contains(QStringLiteral("division by zero")));
        QVERIFY(!c.evaluate(QStringLiteral("√(−4)")).ok);
        QVERIFY(!c.evaluate(QStringLiteral("2+3)")).ok); // a ")" too many
        QVERIFY(!c.evaluate(QStringLiteral("2××3")).ok);  // not 2^3
        QVERIFY(!c.evaluate(QStringLiteral("8÷ ÷2")).ok);
        QVERIFY(!c.evaluate(QStringLiteral("2+")).ok);
        const Calculator::Result unknown = c.evaluate(QStringLiteral("2x"));
        QVERIFY(!unknown.ok);
        QVERIFY(unknown.error.contains(QStringLiteral("x")));
        QVERIFY(!c.evaluate(QStringLiteral("171!")).ok); // too large
    }

    void ansAndHistory()
    {
        Calculator c;
        QCOMPARE(c.calculate(QStringLiteral("12×7")).value, 84.0);
        QCOMPARE(c.ans(), 84.0);
        QCOMPARE(c.calculate(QStringLiteral("Ans÷4")).value, 21.0);
        QCOMPARE(c.calculate(QStringLiteral("ans−30")).value, -9.0);
        QCOMPARE(c.calculate(QStringLiteral("Ans²")).value, 81.0); // negative Ans is bracketed
        QCOMPARE(c.history().size(), 4);
        QCOMPARE(c.history().first().expression, QStringLiteral("(−9)²"));
        QCOMPARE(c.history().first().result, QStringLiteral("81"));
        QCOMPARE(c.history().last().expression, QStringLiteral("12×7"));
        // A failed calculation changes nothing.
        QVERIFY(!c.calculate(QStringLiteral("1÷0")).ok);
        QCOMPARE(c.ans(), 81.0);
        QCOMPARE(c.history().size(), 4);
        // Ans keeps full precision.
        c.calculate(QStringLiteral("1÷3"));
        QCOMPARE(c.calculate(QStringLiteral("Ans×3")).value, 1.0);
        for (int i = 0; i < 30; ++i)
            c.calculate(QStringLiteral("1+1"));
        QCOMPARE(c.history().size(), Calculator::kMaxHistory);
        c.clearHistory();
        QVERIFY(c.history().isEmpty());
    }

    void formatting()
    {
        QCOMPARE(Calculator::format(84), QStringLiteral("84"));
        QCOMPARE(Calculator::format(-2.5), QStringLiteral("−2.5"));
        QCOMPARE(Calculator::format(-0.0), QStringLiteral("0"));
        QCOMPARE(Calculator::format(1099511627776.0), QStringLiteral("1099511627776")); // 2^40 in full
        QCOMPARE(Calculator::format(1.5e15), QStringLiteral("1.5 × 10¹⁵"));
        QCOMPARE(Calculator::format(2.5e-7), QStringLiteral("2.5 × 10⁻⁷"));
        QCOMPARE(Calculator::format(123456789.123456789), QStringLiteral("123456789.123"));
        // Results read back at full precision.
        Calculator c;
        QCOMPARE(value(c, Calculator::toInput(1.5e15)), 1.5e15);
        QCOMPARE(value(c, Calculator::toInput(-2.5e-7)), -2.5e-7);
        QCOMPARE(value(c, Calculator::toInput(1.0 / 3.0)), 1.0 / 3.0);
    }

    void boardEquation()
    {
        QCOMPARE(Calculator::toLatex(QStringLiteral("12×7"), QStringLiteral("84")), QStringLiteral("12 \\times 7 = 84"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("√(9)+3²"), QStringLiteral("12")),
                 QStringLiteral("\\sqrt{9} + 3^{2} = 12"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("sin 30"), QStringLiteral("0.5")), QStringLiteral("\\sin 30 = 0.5"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("2^(1+2)÷4"), QStringLiteral("2")),
                 QStringLiteral("2^{1 + 2} \\div 4 = 2"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("15%×80"), QStringLiteral("12")), QStringLiteral("15\\% \\times 80 = 12"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("2π"), QStringLiteral("6.28318530718")),
                 QStringLiteral("2\\pi = 6.28318530718"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("5−8"), QStringLiteral("−3")), QStringLiteral("5 - 8 = -3"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("10^15×1.5"), QStringLiteral("1.5 × 10¹⁵")),
                 QStringLiteral("10^{15} \\times 1.5 = 1.5 \\times 10^{15}"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("(2+3)×4"), QStringLiteral("20")),
                 QStringLiteral("\\left(2 + 3\\right) \\times 4 = 20"));
        QCOMPARE(Calculator::toLatex(QStringLiteral("asin 1"), QStringLiteral("90")), QStringLiteral("\\arcsin 1 = 90"));
        // The equation renderer lays them out (it typesets \\times, \\sqrt, \\pi, \\sin ... as symbols).
        cb::mathtype::MathTypesetter ts;
        const auto plain = ts.layout(QStringLiteral("12 \\times 7 = 84"), 40);
        const auto literal = ts.layout(QStringLiteral("12 times 7 = 84"), 40);
        QVERIFY(plain && plain->width > 0);
        QVERIFY(plain->width < literal->width); // \\times became the × sign, not the word
    }
};

QTEST_MAIN(TestCalculator)
#include "tst_calculator.moc"
