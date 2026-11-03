#include "math/Expression.h"

#include <QtTest>

#include <cmath>

using cb::math::Expression;

class TestExpression : public QObject
{
    Q_OBJECT
private:
    static double eval(const QString& text, const QHash<QString, double>& vars = {})
    {
        QString error;
        const Expression e = Expression::compile(text, &error);
        if (!e.isValid())
            qWarning() << text << error;
        return e.evaluate(vars);
    }

private slots:
    void arithmetic_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("expected");
        QTest::newRow("precedence") << "1 + 2 * 3" << 7.0;
        QTest::newRow("parentheses") << "(1 + 2) * 3" << 9.0;
        QTest::newRow("power right assoc") << "2^3^2" << 512.0;
        QTest::newRow("unary minus") << "-3^2" << -9.0;
        QTest::newRow("negative exponent") << "2^-1" << 0.5;
        QTest::newRow("unicode") << "3×4 − 2÷2" << 11.0;
        QTest::newRow("squared") << "5²" << 25.0;
        QTest::newRow("functions") << "sqrt(16) + abs(-2)" << 6.0;
        QTest::newRow("absolute bars") << "|2 - 5| + 1" << 4.0;
        QTest::newRow("constants") << "cos(pi)" << -1.0;
        QTest::newRow("implicit") << "2(3 + 1)" << 8.0;
        QTest::newRow("implicit brackets") << "(1+1)(2+1)" << 6.0;
        QTest::newRow("min max") << "max(2, 7) - min(4, 1)" << 6.0;
        QTest::newRow("root sign") << "√9 + 1" << 4.0;
        QTest::newRow("cube root of negative") << "(-8)^(1/3)" << -2.0;
        QTest::newRow("log") << "log(1000) + ln(e)" << 4.0;
    }
    void arithmetic()
    {
        QFETCH(QString, text);
        QFETCH(double, expected);
        QVERIFY2(std::abs(eval(text) - expected) < 1e-9, qPrintable(text));
    }

    void variables()
    {
        const Expression e = Expression::compile(QStringLiteral("a*x^2 + bx + c"));
        QVERIFY(e.isValid());
        QVERIFY(e.usesVariable(QStringLiteral("x")));
        QVERIFY(e.usesVariable(QStringLiteral("a")));
        QVERIFY(e.usesVariable(QStringLiteral("b")));
        QVERIFY(e.usesVariable(QStringLiteral("c")));
        QCOMPARE(e.evaluate({{QStringLiteral("x"), 2}, {QStringLiteral("a"), 1}, {QStringLiteral("b"), 3}, {QStringLiteral("c"), -1}}), 9.0);
    }

    void implicitFunctions()
    {
        QVERIFY(std::abs(eval(QStringLiteral("2sin(x)"), {{QStringLiteral("x"), M_PI / 2}}) - 2.0) < 1e-9);
        QVERIFY(std::abs(eval(QStringLiteral("sinx"), {{QStringLiteral("x"), M_PI / 2}}) - 1.0) < 1e-9);
        QVERIFY(std::abs(eval(QStringLiteral("sin x"), {{QStringLiteral("x"), M_PI / 2}}) - 1.0) < 1e-9);
    }

    void errors_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << "";
        QTest::newRow("open paren") << "(1 + 2";
        QTest::newRow("dangling op") << "1 +";
        QTest::newRow("bad char") << "2 $ 3";
        QTest::newRow("missing arg") << "max(1)";
        QTest::newRow("close paren") << "1 + 2)";
    }
    void errors()
    {
        QFETCH(QString, text);
        QString error;
        const Expression e = Expression::compile(text, &error);
        QVERIFY(!e.isValid());
        QVERIFY(!error.isEmpty());
    }

    void definitions()
    {
        QString name;
        QCOMPARE(Expression::stripDefinition(QStringLiteral("f(x) = x^2"), &name), QStringLiteral("x^2"));
        QCOMPARE(name, QStringLiteral("f"));
        QCOMPARE(Expression::stripDefinition(QStringLiteral("y = 2x + 1"), &name), QStringLiteral("2x + 1"));
        QCOMPARE(name, QStringLiteral("y"));
        QCOMPARE(Expression::stripDefinition(QStringLiteral("x^2"), &name), QStringLiteral("x^2"));
        QVERIFY(name.isEmpty());
    }

    void discontinuity()
    {
        const Expression e = Expression::compile(QStringLiteral("1/x"));
        QVERIFY(std::isinf(e.evaluate({{QStringLiteral("x"), 0.0}})));
    }
};

QTEST_GUILESS_MAIN(TestExpression)
#include "tst_expression.moc"
