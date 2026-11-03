// Scale system and exact numeric geometry: unit conversion, drawing scale, exact lengths, angles,
// slope values, vectors, shape sizes, area and perimeter.
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "geometry/Precision.h"
#include "graph/GraphObject.h"
#include "math/CoordinateSystem.h"
#include "math/MeasureScale.h"

#include <QtTest>

using namespace cb;

namespace {
constexpr double kEps = 1e-9;

MeasureScale scale(double board, const char* boardUnit, double real, const char* realUnit)
{
    MeasureScale s;
    s.boardValue = board;
    s.boardUnit = QString::fromLatin1(boardUnit);
    s.realValue = real;
    s.realUnit = QString::fromLatin1(realUnit);
    return s;
}

CoordinateSystem boardCs(const MeasureScale& s = MeasureScale())
{
    CoordinateSystem cs = CoordinateSystem::global(QPointF(960, 540), 40.0, QStringLiteral("cm"));
    cs.setScale(s);
    return cs;
}

/// Page points of a horizontal line of the given length in cm starting at (200, 500).
QVector<QPointF> lineCm(double cm)
{
    return {QPointF(200, 500), QPointF(200 + cm * 40.0, 500)};
}

template <typename T>
T* addObject(Document& doc, std::unique_ptr<T> object)
{
    T* raw = object.get();
    std::vector<ObjectPtr> v;
    v.push_back(std::move(object));
    doc.commands().push(std::make_unique<AddObjectsCommand>(doc.currentPage()->id(), std::move(v)));
    return raw;
}
} // namespace

class TestPrecision : public QObject
{
    Q_OBJECT
private slots:
    void unitConversion()
    {
        QCOMPARE(units::convert(1, QStringLiteral("km"), QStringLiteral("m")), 1000.0);
        QCOMPARE(units::convert(250, QStringLiteral("cm"), QStringLiteral("m")), 2.5);
        QCOMPARE(units::convert(1, QStringLiteral("in"), QStringLiteral("cm")), 2.54);
        QVERIFY(std::abs(units::convert(1, QStringLiteral("mi"), QStringLiteral("km")) - 1.609344) < kEps);
        QCOMPARE(units::convert(3, QStringLiteral("ft"), QStringLiteral("yd")), 1.0);
        QVERIFY(!units::isLengthUnit(QStringLiteral("parsec")));
        QCOMPARE(units::lengthUnits().size(), 8);
    }

    void scaleDefinitions_data()
    {
        QTest::addColumn<double>("board");
        QTest::addColumn<QString>("boardUnit");
        QTest::addColumn<double>("real");
        QTest::addColumn<QString>("realUnit");
        QTest::addColumn<double>("drawnCm");
        QTest::addColumn<double>("expectedReal");
        QTest::newRow("10 cm = 1 m") << 10.0 << "cm" << 1.0 << "m" << 7.0 << 0.7;
        QTest::newRow("10 cm = 1 km") << 10.0 << "cm" << 1.0 << "km" << 7.0 << 0.7;
        QTest::newRow("5 cm = 1 km") << 5.0 << "cm" << 1.0 << "km" << 10.0 << 2.0;
        QTest::newRow("1 cm = 10 m") << 1.0 << "cm" << 10.0 << "m" << 7.0 << 70.0;
        QTest::newRow("1 mm = 1 m") << 1.0 << "mm" << 1.0 << "m" << 1.0 << 10.0;
        QTest::newRow("1 in = 1 mi") << 1.0 << "in" << 1.0 << "mi" << 2.54 << 1.0;
    }
    void scaleDefinitions()
    {
        QFETCH(double, board);
        QFETCH(QString, boardUnit);
        QFETCH(double, real);
        QFETCH(QString, realUnit);
        QFETCH(double, drawnCm);
        QFETCH(double, expectedReal);
        MeasureScale s;
        s.boardValue = board;
        s.boardUnit = boardUnit;
        s.realValue = real;
        s.realUnit = realUnit;
        QVERIFY(s.isValid());
        QVERIFY(!s.isIdentity());
        const CoordinateSystem cs = boardCs(s);
        QVERIFY(cs.usesScale());
        QCOMPARE(cs.realUnitLabel(), realUnit);
        // Visual -> mathematical.
        QVERIFY(std::abs(cs.toReal(drawnCm) - expectedReal) < 1e-9);
        // Mathematical -> visual (bidirectional).
        QVERIFY(std::abs(cs.fromReal(expectedReal) - drawnCm) < 1e-9);
    }

    void identityScaleShowsBoardUnits()
    {
        const CoordinateSystem cs = boardCs();
        QVERIFY(!cs.usesScale());
        QCOMPARE(cs.formatLength(7.0), QStringLiteral("7 cm"));
        // 10 mm = 1 cm has factor 1 but a different display unit, so it still counts as a scale.
        QVERIFY(!scale(10, "mm", 1, "cm").isIdentity());
        QVERIFY(std::abs(scale(10, "mm", 1, "cm").factor() - 1.0) < kEps);
        // Invalid scales in a file fall back to 1:1.
        QVERIFY(MeasureScale::fromJson(QJsonObject{{QStringLiteral("board"), -1}}).isIdentity());
        // Graph axes are unitless: a scale never applies to them.
        CoordinateSystem graph = CoordinateSystem::fromTransform(QTransform(), QString());
        graph.setScale(scale(10, "cm", 1, "km"));
        QVERIFY(!graph.usesScale());
        QCOMPARE(graph.toReal(3.0), 3.0);
    }

    void scaleBasedSquare()
    {
        // 10 cm = 1 km: a 10 cm square is 1 km × 1 km, area 1 km², perimeter 4 km.
        const CoordinateSystem cs = boardCs(scale(10, "cm", 1, "km"));
        const QVector<QPointF> square = {QPointF(100, 100), QPointF(500, 100), QPointF(500, 500), QPointF(100, 500)};
        QVERIFY(std::abs(cs.toReal(cs.mathDistance(square[0], square[1])) - 1.0) < kEps);
        QVERIFY(std::abs(cs.toRealArea(cs.mathArea(square)) - 1.0) < kEps);
        QVERIFY(std::abs(cs.toReal(MeasurementObject::perimeter(square, cs)) - 4.0) < kEps);
        QCOMPARE(MeasurementObject::format(MeasureKind::Area, square, cs), QStringLiteral("A = 1 km²  ·  P = 4 km"));
        // 100 cm × 100 cm: 10 km × 10 km, area 100 km², perimeter 40 km.
        const QVector<QPointF> big = {QPointF(0, 0), QPointF(4000, 0), QPointF(4000, 4000), QPointF(0, 4000)};
        QVERIFY(std::abs(cs.toReal(cs.mathDistance(big[0], big[1])) - 10.0) < kEps);
        QVERIFY(std::abs(cs.toRealArea(cs.mathArea(big)) - 100.0) < kEps);
        QVERIFY(std::abs(cs.toReal(MeasurementObject::perimeter(big, cs)) - 40.0) < kEps);
    }

    void shapeMetricsUseTheScale()
    {
        Document doc;
        doc.commands().push(std::make_unique<SetScaleCommand>(scale(10, "cm", 1, "km"), QString()));
        auto* square = addObject(doc, ShapeObject::createBox(ShapeKind::Rectangle, QRectF(200, 200, 400, 400), ShapeStyle()));
        const CoordinateSystem cs = precision::coordinateSystemFor(doc, doc.currentPage(), *square);
        precision::ShapeMetrics m;
        QVERIFY(precision::shapeMetrics(*square, cs, &m));
        QCOMPARE(m.width, 10.0);
        QCOMPARE(m.height, 10.0);
        QVERIFY(std::abs(cs.toReal(m.width) - 1.0) < kEps);
        QVERIFY(std::abs(cs.toRealArea(m.area) - 1.0) < kEps);
        QVERIFY(std::abs(cs.toReal(m.perimeter) - 4.0) < kEps);
        // Exact size entry: 2 km × 0.5 km.
        const ObjectId squareId = square->id(); // the edit replaces the instance
        QVERIFY(precision::applyShapeSize(doc, *doc.currentPage(), squareId, cs.fromReal(2.0), cs.fromReal(0.5), cs,
                                          QStringLiteral("Size")));
        auto* resized = doc.currentPage()->object(squareId);
        QVERIFY(precision::shapeMetrics(*resized, cs, &m));
        QVERIFY(std::abs(cs.toReal(m.width) - 2.0) < kEps);
        QVERIFY(std::abs(cs.toReal(m.height) - 0.5) < kEps);
        QVERIFY(std::abs(cs.toRealArea(m.area) - 1.0) < kEps);
        QVERIFY(std::abs(cs.toReal(m.perimeter) - 5.0) < kEps);
        // Circles use the exact formulas.
        auto* circle = addObject(doc, ShapeObject::createBox(ShapeKind::Circle, QRectF(0, 0, 400, 400), ShapeStyle()));
        QVERIFY(precision::shapeMetrics(*circle, cs, &m));
        QVERIFY(std::abs(m.area - geom::kPi * 25.0) < 1e-6);
        QVERIFY(std::abs(m.perimeter - geom::kPi * 10.0) < 1e-6);
        // A right triangle 10 × 10 cm.
        auto* tri = addObject(doc, ShapeObject::createBox(ShapeKind::RightTriangle, QRectF(0, 0, 400, 400), ShapeStyle()));
        QVERIFY(precision::shapeMetrics(*tri, cs, &m));
        QVERIFY(std::abs(m.area - 50.0) < 1e-6);
        QVERIFY(std::abs(m.perimeter - (20.0 + std::sqrt(200.0))) < 1e-6);
    }

    void exactLineLength_data()
    {
        QTest::addColumn<double>("cm");
        QTest::newRow("7 cm") << 7.0;
        QTest::newRow("8 cm") << 8.0;
        QTest::newRow("0.5 cm") << 0.5;
        QTest::newRow("123.4 cm") << 123.4;
    }
    void exactLineLength()
    {
        QFETCH(double, cm);
        const CoordinateSystem cs = boardCs();
        // A slanted, hand-drawn line.
        const QVector<QPointF> drawn = {QPointF(300, 400), QPointF(517, 263)};
        const double dir = precision::direction(drawn, cs);
        const QVector<QPointF> exact = precision::withLength(drawn, cm, cs);
        QCOMPARE(exact[0], drawn[0]); // start point stays
        QVERIFY(std::abs(precision::length(exact, cs) - cm) < 1e-9);
        QVERIFY(std::abs(precision::direction(exact, cs) - dir) < 1e-9); // direction preserved
    }

    void exactLengthOnObjectsIsUndoable()
    {
        Document doc;
        auto* line = addObject(doc, ShapeObject::createLine(ShapeKind::Line, QPointF(100, 100), QPointF(390, 180), ShapeStyle()));
        auto* segment = addObject(doc, GeometryObject::create(ConstructKind::Segment, {QPointF(100, 600), QPointF(300, 610)}, Qt::cyan, true));
        auto* distance = addObject(doc, MeasurementObject::create(MeasureKind::Distance, {QPointF(500, 900), QPointF(830, 900)}, Qt::yellow));
        Page& page = *doc.currentPage();
        const ObjectId distanceId = distance->id();
        for (const ObjectId& id : {line->id(), segment->id(), distance->id()}) {
            DocumentObject* o = page.object(id);
            const CoordinateSystem cs = precision::coordinateSystemFor(doc, &page, *o);
            const QVector<QPointF> pts = precision::definingPoints(*o);
            QCOMPARE(pts.size(), 2);
            QVERIFY(precision::applyPoints(doc, page, id, precision::withLength(pts, 7.0, cs), QStringLiteral("Length")));
            const QVector<QPointF> after = precision::definingPoints(*page.object(id));
            QVERIFY(std::abs(precision::length(after, cs) - 7.0) < 1e-9);
            QVERIFY(geom::distance(after[0], pts[0]) < 1e-9);
            doc.commands().undo();
            QVERIFY(geom::distance(precision::definingPoints(*page.object(id))[1], pts[1]) < 1e-9);
            doc.commands().redo();
        }
        // The measurement label follows the new length.
        const CoordinateSystem cs = precision::coordinateSystemFor(doc, &page, *page.object(distanceId));
        QCOMPARE(static_cast<MeasurementObject*>(page.object(distanceId))->valueText(cs), QStringLiteral("7 cm"));
    }

    void realLengthWithScale()
    {
        // Scale 10 cm = 1 km; enter 1 km -> the board line becomes exactly 10 cm.
        const CoordinateSystem cs = boardCs(scale(10, "cm", 1, "km"));
        QVector<QPointF> pts = lineCm(7);
        QCOMPARE(cs.formatLength(precision::length(pts, cs)), QStringLiteral("0.7 km"));
        pts = precision::withLength(pts, cs.fromReal(1.0), cs);
        QVERIFY(std::abs(precision::length(pts, cs) - 10.0) < 1e-9);
        QVERIFY(std::abs(cs.toReal(precision::length(pts, cs)) - 1.0) < 1e-12);
        QVERIFY(std::abs(pts[1].x() - (200 + 400)) < 1e-9);
    }

    void exactAngles_data()
    {
        QTest::addColumn<double>("degrees");
        for (double a : {0.0, 30.0, 45.0, 60.0, 90.0, 120.0, 135.0, 180.0, 17.5})
            QTest::newRow(qPrintable(QString::number(a))) << a;
    }
    void exactAngles()
    {
        QFETCH(double, degrees);
        const CoordinateSystem cs = boardCs();
        // Arm A towards the right, arm B roughly up-right (opens counter-clockwise).
        const QVector<QPointF> drawn = {QPointF(700, 500), QPointF(500, 500), QPointF(620, 380)};
        const QVector<QPointF> exact = precision::withAngle(drawn, degrees, cs);
        QCOMPARE(exact[0], drawn[0]);
        QCOMPARE(exact[1], drawn[1]);
        QVERIFY2(std::abs(precision::angle(exact, cs) - degrees) < 1e-9,
                 qPrintable(QString::number(precision::angle(exact, cs))));
        // Arm length preserved.
        QVERIFY(std::abs(geom::distance(exact[2], exact[1]) - geom::distance(drawn[2], drawn[1])) < 1e-9);
        // The angle measurement label reads the exact value.
        QCOMPARE(MeasurementObject::format(MeasureKind::Angle, exact, cs), geom::formatNumber(degrees, 1) + QStringLiteral("°"));
    }

    void angleKeepsItsSide()
    {
        const CoordinateSystem cs = boardCs();
        // Opens clockwise (arm B below arm A on screen = negative math direction).
        const QVector<QPointF> drawn = {QPointF(700, 500), QPointF(500, 500), QPointF(620, 620)};
        const QVector<QPointF> exact = precision::withAngle(drawn, 90.0, cs);
        QVERIFY(std::abs(precision::angle(exact, cs) - 90.0) < 1e-9);
        QVERIFY(exact[2].y() > 500); // still below
        QVERIFY(std::abs(exact[2].x() - 500) < 1e-9);
    }

    void slopeValues()
    {
        const CoordinateSystem cs = boardCs();
        const QVector<QPointF> base = {QPointF(400, 600), QPointF(500, 560)};
        // Rise = 4, Run = 2 -> slope 2, inclination 63.435°.
        QVector<QPointF> pts = precision::withRiseRun(base, 4, 2, cs);
        auto s = precision::slopeValues(pts, cs);
        QVERIFY(std::abs(s.rise - 4) < kEps);
        QVERIFY(std::abs(s.run - 2) < kEps);
        QVERIFY(std::abs(s.slope - 2) < kEps);
        QVERIFY(std::abs(s.angle - 63.43494882292201) < 1e-9);
        QCOMPARE(MeasurementObject::format(MeasureKind::Slope, pts, cs), QStringLiteral("m = 2"));
        // The dashed reference triangle follows: corner is (pts1.x, pts0.y) with legs run and rise.
        QVERIFY(std::abs((pts[1].x() - pts[0].x()) - 80) < 1e-9); // 2 cm
        QVERIFY(std::abs((pts[0].y() - pts[1].y()) - 160) < 1e-9); // 4 cm up
        // Slope = -0.5 keeps the run.
        pts = precision::withSlope(pts, -0.5, cs);
        s = precision::slopeValues(pts, cs);
        QVERIFY(std::abs(s.run - 2) < kEps);
        QVERIFY(std::abs(s.rise + 1) < kEps);
        // Angle = 45° keeps the length.
        const double len = precision::length(pts, cs);
        pts = precision::withInclination(pts, 45, cs);
        s = precision::slopeValues(pts, cs);
        QVERIFY(std::abs(s.slope - 1) < 1e-9);
        QVERIFY(std::abs(s.angle - 45) < 1e-9);
        QVERIFY(std::abs(precision::length(pts, cs) - len) < 1e-9);
        // Vertical: slope undefined.
        pts = precision::withRiseRun(pts, 3, 0, cs);
        s = precision::slopeValues(pts, cs);
        QVERIFY(s.vertical);
        QCOMPARE(MeasurementObject::format(MeasureKind::Slope, pts, cs), QObject::tr("m undefined"));
    }

    void vectorMagnitudeAndDirection()
    {
        // 2 km at 90° with 10 cm = 1 km is a 20 cm arrow pointing straight up.
        Document doc;
        doc.commands().push(std::make_unique<SetScaleCommand>(scale(10, "cm", 1, "km"), QString()));
        auto* v = addObject(doc, GeometryObject::create(ConstructKind::Vector, {QPointF(900, 800), QPointF(1010, 760)}, Qt::cyan, true));
        const ObjectId vid = v->id();
        Page& page = *doc.currentPage();
        const CoordinateSystem cs = precision::coordinateSystemFor(doc, &page, *v);
        QVERIFY(cs.usesScale());
        const QVector<QPointF> pts = precision::withPolar(precision::definingPoints(*v), cs.fromReal(2.0), 90.0, cs);
        QVERIFY(precision::applyPoints(doc, page, vid, pts, QStringLiteral("Vector")));
        const QVector<QPointF> after = precision::definingPoints(*page.object(vid));
        QVERIFY(std::abs(cs.toReal(precision::length(after, cs)) - 2.0) < 1e-12);
        QVERIFY(std::abs(precision::direction(after, cs) - 90.0) < 1e-9);
        QVERIFY(std::abs(after[1].x() - 900) < 1e-9);
        QVERIFY(std::abs((after[0].y() - after[1].y()) - 800) < 1e-9); // 20 cm on the board
        QVERIFY(static_cast<GeometryObject*>(page.object(vid))->labelText(cs).contains(QStringLiteral("|v| = 2 km")));
        // Direction only.
        const QVector<QPointF> west = precision::withDirection(after, 180.0, cs);
        QVERIFY(std::abs(precision::direction(west, cs) - 180.0) < 1e-9);
        QVERIFY(std::abs(cs.toReal(precision::length(west, cs)) - 2.0) < 1e-12);
    }

    void graphObjectsUseGraphUnits()
    {
        // Inside a graph, vectors and slopes use the graph's axes and ignore the drawing scale.
        Document doc;
        doc.commands().push(std::make_unique<SetScaleCommand>(scale(10, "cm", 1, "km"), QString()));
        auto* graph = addObject(doc, GraphObject::create(QPointF(960, 540), QSizeF(800, 800)));
        const CoordinateSystem* gcs = graph->mathCoordinateSystem();
        QVERIFY(gcs);
        const QPointF o = gcs->toPage(QPointF(0, 0));
        auto* v = addObject(doc, GeometryObject::create(ConstructKind::Vector, {o, gcs->toPage(QPointF(1, 1))}, Qt::cyan, true));
        const CoordinateSystem cs = precision::coordinateSystemFor(doc, doc.currentPage(), *v);
        QVERIFY(!cs.usesScale());
        QVERIFY(std::abs(precision::length(precision::definingPoints(*v), cs) - std::sqrt(2.0)) < 1e-9);
        const QVector<QPointF> pts = precision::withRiseRun(precision::definingPoints(*v), 4, 2, cs);
        QVERIFY(std::abs(cs.toMath(pts[1]).x() - 2) < 1e-9);
        QVERIFY(std::abs(cs.toMath(pts[1]).y() - 4) < 1e-9);
    }

    void scaleChangeIsUndoableAndUpdatesLabels()
    {
        Document doc;
        auto* d = addObject(doc, MeasurementObject::create(MeasureKind::Distance, lineCm(7), Qt::yellow));
        QSignalSpy spy(&doc, &Document::coordinatesChanged);
        auto label = [&]() {
            const CoordinateSystem cs = precision::coordinateSystemFor(doc, doc.currentPage(), *d);
            return d->valueText(cs);
        };
        QCOMPARE(label(), QStringLiteral("7 cm"));
        doc.commands().push(std::make_unique<SetScaleCommand>(scale(10, "cm", 1, "km"), QStringLiteral("Scale")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(label(), QStringLiteral("0.7 km"));
        doc.commands().undo();
        QCOMPARE(label(), QStringLiteral("7 cm"));
        QCOMPARE(spy.count(), 2);
    }

    void measurementsAreIndependentOfScreenAndZoom()
    {
        // Values come from document coordinates only: the same page points give the same value
        // whatever view transform (zoom 25 %..250 %, any DPI) is used to display them.
        const CoordinateSystem cs = boardCs();
        const QVector<QPointF> pts = lineCm(7);
        const QString expected = MeasurementObject::format(MeasureKind::Distance, pts, cs);
        for (qreal zoom : {0.25, 0.5, 1.0, 2.5}) {
            QTransform view;
            view.translate(37, 11);
            view.scale(zoom, zoom);
            // Round-trip through view (screen) coordinates as the input system does.
            QVector<QPointF> roundTrip;
            for (const QPointF& p : pts)
                roundTrip.push_back(view.inverted().map(view.map(p)));
            QCOMPARE(MeasurementObject::format(MeasureKind::Distance, roundTrip, cs), expected);
        }
        QCOMPARE(expected, QStringLiteral("7 cm"));
    }
};

QTEST_MAIN(TestPrecision)
#include "tst_precision.moc"
