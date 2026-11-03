// Magic Equation Maker: offline handwriting recognition (symbols, superscripts, fractions,
// rejection of scribbles) and the explicit Accept / Edit / Cancel workflow.
#include "ai/MathInkRecognizer.h"
#include "SyntheticInk.h"
#include "core/Geometry.h"

#include <QRandomGenerator>
#include <QtTest>

#include <cmath>

using namespace cb;

using namespace testink;

namespace {
QString recognizeLatex(const Strokes& ink, double* confidence = nullptr, bool relaxed = false)
{
    MathInkRecognizer::Options options;
    options.relaxed = relaxed;
    const MathInkRecognizer r(options);
    const auto candidates = r.recognize(InkSample{ink});
    if (candidates.isEmpty())
        return QString();
    if (confidence)
        *confidence = candidates.first().confidence;
    return candidates.first().latex;
}
} // namespace

class TestMagic : public QObject
{
    Q_OBJECT
private slots:
    void singleSymbols_data()
    {
        QTest::addColumn<QString>("symbol");
        for (const char* s : {"0", "1", "2", "3", "4", "5", "7", "x", "y", "+", "(", ")"})
            QTest::newRow(s) << QString::fromLatin1(s);
    }
    void singleSymbols()
    {
        QFETCH(QString, symbol);
        const MathInkRecognizer r;
        int correct = 0;
        for (quint32 seed = 1; seed <= 20; ++seed) {
            Writer w(seed);
            double score = 0;
            const QString got = r.classifySymbol(w.symbol(symbol.at(0), QRectF(0, 0, 50, 70)), &score);
            correct += got == symbol;
        }
        QVERIFY2(correct >= 18, qPrintable(QStringLiteral("%1/20 correct").arg(correct)));
    }

    void expressions_data()
    {
        QTest::addColumn<QString>("written");
        QTest::addColumn<QString>("latex");
        QTest::newRow("x² + 1") << "x^2 + 1" << "x^{2}+1";
        QTest::newRow("2x + 3") << "2x + 3" << "2x+3";
        QTest::newRow("y = ... uses minus") << "5 - 3" << "5-3";
        QTest::newRow("(x + 1)²") << "(x + 1)^2" << "(x+1)^{2}";
        QTest::newRow("40") << "40" << "40";
        QTest::newRow("x³") << "x^3" << "x^{3}";
    }
    void expressions()
    {
        QFETCH(QString, written);
        QFETCH(QString, latex);
        for (quint32 seed = 1; seed <= 5; ++seed) {
            Writer w(seed * 7);
            double confidence = 0;
            const QString got = recognizeLatex(write(w, written), &confidence);
            QVERIFY2(got == latex, qPrintable(QStringLiteral("seed %1: got '%2' expected '%3'").arg(seed).arg(got, latex)));
            QVERIFY(confidence > 0.0 && confidence <= 1.0);
        }
    }

    void equalsSign()
    {
        Writer w(3);
        Strokes ink = write(w, QStringLiteral("2x"));
        ink += w.bar(250, 290, 318);
        ink += w.bar(250, 290, 338);
        ink += write(w, QStringLiteral("4"), QPointF(310, 300));
        QCOMPARE(recognizeLatex(ink), QStringLiteral("2x=4"));
    }

    void fraction()
    {
        Writer w(5);
        Strokes ink = w.symbol(QLatin1Char('1'), QRectF(120, 200, 22, 50));
        ink += w.bar(100, 170, 270);
        ink += w.symbol(QLatin1Char('2'), QRectF(115, 285, 36, 50));
        QCOMPARE(recognizeLatex(ink), QStringLiteral("\\frac{1}{2}"));
        // x + 1/2
        Strokes sum = write(w, QStringLiteral("x +"), QPointF(-40, 240));
        sum += ink;
        QCOMPARE(recognizeLatex(sum), QStringLiteral("x+\\frac{1}{2}"));
    }

    void scribbleIsRejected()
    {
        // A zig-zag scribble and a random tangle are not mathematics: strict recognition fails
        // (the UI then offers Try again / Edit manually / Cancel).
        QVector<QPointF> zigzag;
        for (int i = 0; i < 40; ++i)
            zigzag << QPointF(100 + i * 8, (i % 2) ? 100 : 180);
        QVector<QPointF> tangle;
        QRandomGenerator rng(42);
        for (int i = 0; i < 60; ++i)
            tangle << QPointF(300 + rng.bounded(120), 100 + rng.bounded(120));
        QVERIFY(recognizeLatex({zigzag}).isEmpty());
        QVERIFY(recognizeLatex({tangle}).isEmpty());
        // Relaxed mode ("Try again") still offers a low-confidence guess for the teacher to judge.
        double confidence = 1.0;
        QVERIFY(!recognizeLatex({tangle}, &confidence, true).isEmpty());
        QVERIFY(confidence < 0.6);
    }

    void emptyInk()
    {
        QVERIFY(recognizeLatex({}).isEmpty());
        QVERIFY(recognizeLatex({QVector<QPointF>()}).isEmpty());
    }

    void translationAndScaleInvariant()
    {
        Writer w(11);
        const Strokes ink = write(w, QStringLiteral("x^2 + 1"));
        for (double s : {0.3, 1.0, 4.0}) {
            Strokes scaled;
            for (const auto& stroke : ink) {
                QVector<QPointF> t;
                for (const QPointF& p : stroke)
                    t << p * s + QPointF(1000, -500);
                scaled << t;
            }
            QCOMPARE(recognizeLatex(scaled), QStringLiteral("x^{2}+1"));
        }
    }
};

QTEST_MAIN(TestMagic)
#include "tst_magic.moc"
