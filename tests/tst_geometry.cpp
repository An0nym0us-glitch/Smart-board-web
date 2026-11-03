#include "core/Crc32.h"
#include "core/Geometry.h"
#include "math/CoordinateSystem.h"

#include <QtTest>

using namespace cb;

class TestGeometry : public QObject
{
    Q_OBJECT
private slots:
    void distanceToSegment()
    {
        QCOMPARE(geom::distanceToSegment(QPointF(0, 5), QPointF(-10, 0), QPointF(10, 0)), 5.0);
        QCOMPARE(geom::distanceToSegment(QPointF(13, 4), QPointF(-10, 0), QPointF(10, 0)), 5.0);
    }
    void segmentCircle()
    {
        double t0 = 0, t1 = 0;
        QVERIFY(geom::segmentCircleInterval(QPointF(-10, 0), QPointF(10, 0), QPointF(0, 0), 5, t0, t1));
        QVERIFY(qAbs(t0 - 0.25) < 1e-9);
        QVERIFY(qAbs(t1 - 0.75) < 1e-9);
        QVERIFY(!geom::segmentCircleInterval(QPointF(-10, 10), QPointF(10, 10), QPointF(0, 0), 5, t0, t1));
    }
    void angles()
    {
        QCOMPARE(geom::normalizeDegrees(-90), 270.0);
        QCOMPARE(geom::angleDifference(350, 10), 20.0);
        QCOMPARE(geom::angleDifference(10, 350), -20.0);
        bool snapped = false;
        QCOMPARE(geom::snapAngle(44.2, 15, 2, &snapped), 45.0);
        QVERIFY(snapped);
    }
    void polygon()
    {
        QCOMPARE(geom::polygonArea({QPointF(0, 0), QPointF(4, 0), QPointF(4, 3), QPointF(0, 3)}), 12.0);
        QCOMPARE(geom::polylineLength({QPointF(0, 0), QPointF(3, 4)}), 5.0);
    }
    void simplify()
    {
        QVector<QPointF> line;
        for (int i = 0; i <= 100; ++i)
            line.push_back(QPointF(i, (i % 2) * 0.01));
        const QVector<int> kept = geom::simplifyIndices(line, 0.1);
        QCOMPARE(kept.size(), 2);
    }
    void formatting()
    {
        QCOMPARE(geom::formatNumber(2.5), QStringLiteral("2.5"));
        QCOMPARE(geom::formatNumber(3.0), QStringLiteral("3"));
        QCOMPARE(geom::formatNumber(-1.24, 1), QString(QChar(0x2212)) + QStringLiteral("1.2"));
        QCOMPARE(geom::niceNumber(0.73, true), 1.0);
        QCOMPARE(geom::niceNumber(0.27, true), 0.2);
        QCOMPARE(geom::niceNumber(4.2, true), 5.0);
    }
    void coordinateSystem()
    {
        const CoordinateSystem cs = CoordinateSystem::global(QPointF(960, 540), 40, QStringLiteral("cm"));
        QCOMPARE(cs.toMath(QPointF(1000, 500)), QPointF(1, 1));
        QCOMPARE(cs.toPage(QPointF(-2, 0)), QPointF(880, 540));
        QCOMPARE(cs.mathDistance(QPointF(960, 540), QPointF(1080, 380)), 5.0);
        double slope = 0;
        QVERIFY(cs.mathSlope(QPointF(960, 540), QPointF(1000, 460), &slope));
        QCOMPARE(slope, 2.0);
        QVERIFY(!cs.mathSlope(QPointF(960, 540), QPointF(960, 400), &slope));
        QCOMPARE(cs.pxPerUnit(), 40.0);
        const CoordinateSystem copy = CoordinateSystem::fromJson(cs.toJson());
        QCOMPARE(copy.toMath(QPointF(1000, 500)), QPointF(1, 1));
    }
    void crc()
    {
        QCOMPARE(crc32(QByteArray("123456789")), 0xCBF43926u);
    }
};

QTEST_GUILESS_MAIN(TestGeometry)
#include "tst_geometry.moc"
