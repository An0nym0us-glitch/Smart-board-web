#include "math/equation/EquationObject.h"
#include "math/equation/MathLayout.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

using namespace cb;
using namespace cb::mathtype;

class TestMathLayout : public QObject
{
    Q_OBJECT
private:
    MathTypesetter m_ts;

    /// Renders and returns the number of "ink" pixels, proving something visible was drawn.
    static int inkPixels(const Box& box)
    {
        QImage img(qMax(1, qCeil(box.width) + 20), qMax(1, qCeil(box.height()) + 20), QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        box.paint(p, QPointF(10, 10 + box.ascent), Qt::white);
        p.end();
        int count = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (qRed(img.pixel(x, y)) > 60)
                    ++count;
        return count;
    }

private slots:
    void formulas_data()
    {
        QTest::addColumn<QString>("latex");
        QTest::newRow("fraction") << "\\frac{a+b}{c}";
        QTest::newRow("power") << "x^{2} + y_{i}^{3}";
        QTest::newRow("root") << "\\sqrt{x^2+1} + \\sqrt[3]{8}";
        QTest::newRow("integral") << "\\int_{0}^{1} f(x)\\,dx";
        QTest::newRow("sum") << "\\sum_{i=1}^{n} i = \\frac{n(n+1)}{2}";
        QTest::newRow("limit") << "\\lim_{x \\to 0} \\frac{\\sin x}{x} = 1";
        QTest::newRow("vector") << "\\vec{v} = \\hat{i} + \\bar{x}";
        QTest::newRow("matrix") << "A = \\begin{pmatrix} 1 & 2 \\\\ 3 & 4 \\end{pmatrix}";
        QTest::newRow("cases") << "|x| = \\begin{cases} x & x \\geq 0 \\\\ -x & x < 0 \\end{cases}";
        QTest::newRow("greek") << "\\alpha + \\beta \\neq \\Omega \\pm \\pi";
        QTest::newRow("delimiters") << "\\left( \\frac{1}{2} \\right)^{2}";
        QTest::newRow("text") << "\\text{area} = \\pi r^2";
        QTest::newRow("binomial") << "\\binom{n}{k}";
        QTest::newRow("unicode input") << "a² + b² = c²";
    }
    void formulas()
    {
        QFETCH(QString, latex);
        const BoxPtr box = m_ts.layout(latex, 40);
        QVERIFY(box);
        QVERIFY(box->width > 10);
        QVERIFY(box->height() > 10);
        QVERIFY(inkPixels(*box) > 20);
    }

    void fractionIsTallerThanText()
    {
        const BoxPtr plain = m_ts.layout(QStringLiteral("ab"), 40);
        const BoxPtr frac = m_ts.layout(QStringLiteral("\\frac{a}{b}"), 40);
        QVERIFY(frac->height() > plain->height() * 1.5);
        QVERIFY(frac->descent > 5); // denominator below the baseline
    }

    void superscriptRaised()
    {
        const BoxPtr base = m_ts.layout(QStringLiteral("x"), 40);
        const BoxPtr sup = m_ts.layout(QStringLiteral("x^{2}"), 40);
        QVERIFY(sup->ascent > base->ascent);
        QVERIFY(sup->width > base->width);
    }

    void malformedInputDoesNotCrash_data()
    {
        QTest::addColumn<QString>("latex");
        QTest::newRow("unbalanced open") << "\\frac{a";
        QTest::newRow("unbalanced close") << "a}}b";
        QTest::newRow("unknown command") << "\\foo{x}";
        QTest::newRow("lonely scripts") << "^_^";
        QTest::newRow("empty sqrt") << "\\sqrt";
        QTest::newRow("unterminated env") << "\\begin{pmatrix} 1 & 2";
        QTest::newRow("stray right") << "a \\right) b";
        QTest::newRow("empty") << "";
    }
    void malformedInputDoesNotCrash()
    {
        QFETCH(QString, latex);
        const BoxPtr box = m_ts.layout(latex, 30);
        QVERIFY(box);
        QImage img(200, 100, QImage::Format_ARGB32);
        QPainter p(&img);
        box->paint(p, QPointF(5, 50), Qt::white);
    }

    void equationObjectScales()
    {
        auto e = EquationObject::create(QStringLiteral("E = mc^2"), QPointF(0, 0), 40, Qt::white);
        const QRectF before = e->localBounds();
        e->resizeTo(QRectF(before.left() * 2, before.top() * 2, before.width() * 2, before.height() * 2));
        QCOMPARE(e->pixelSize(), 80.0);
        QVERIFY(e->localBounds().width() > before.width() * 1.7);
    }
};

QTEST_MAIN(TestMathLayout)
#include "tst_mathlayout.moc"
