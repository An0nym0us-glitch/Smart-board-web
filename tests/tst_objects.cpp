#include "ai/ShapeRecognizer.h"
#include "core/Geometry.h"
#include "document/CoordinateResolver.h"
#include "document/ImageStore.h"
#include "document/Page.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/Instruments.h"
#include "geometry/MeasurementObject.h"
#include "graph/GraphObject.h"
#include "graph/TableObject.h"

#include <QtTest>

#include <cmath>

using namespace cb;

class TestObjects : public QObject
{
    Q_OBJECT
private slots:
    void strokeGeometryAndHitTest()
    {
        auto s = StrokeObject::fromPagePoints({{QPointF(100, 100), 1}, {QPointF(200, 100), 1}}, InkStyle());
        QCOMPARE(s->position(), QPointF(150, 100));
        QVERIFY(s->hitTest(QPointF(150, 103), 1.5));
        QVERIFY(!s->hitTest(QPointF(150, 103), 0));
        QVERIFY(!s->hitTest(QPointF(150, 120), 5));
        QVERIFY(s->sceneBounds().contains(QPointF(100, 100)));
    }

    void strokeEraseEnds()
    {
        auto s = StrokeObject::fromPagePoints({{QPointF(0, 0), 1}, {QPointF(100, 0), 1}}, InkStyle());
        bool touched = false;
        auto parts = s->eraseCircle(QPointF(0, 0), 10, &touched);
        QVERIFY(touched);
        QCOMPARE(parts.size(), size_t(1));
        QVERIFY(parts.front()->pagePoints().first().x() > 10);
        parts = s->eraseCircle(QPointF(50, 50), 10, &touched);
        QVERIFY(!touched);
        QVERIFY(parts.empty());
    }

    void rotatedStrokeErase()
    {
        auto s = StrokeObject::fromPagePoints({{QPointF(0, 0), 1}, {QPointF(100, 0), 1}}, InkStyle());
        s->setRotation(90); // now vertical around (50, 0)
        bool touched = false;
        auto parts = s->eraseCircle(QPointF(50, 0), 5, &touched);
        QVERIFY(touched);
        QCOMPARE(parts.size(), size_t(2));
        for (const auto& p : parts)
            for (const QPointF& pt : p->pagePoints())
                QVERIFY(qAbs(pt.x() - 50) < 1e-6);
    }

    void resizeAndRotateShape()
    {
        auto r = ShapeObject::createBox(ShapeKind::Rectangle, QRectF(0, 0, 100, 50), ShapeStyle());
        r->resizeTo(QRectF(-50, -25, 200, 50));
        QCOMPARE(r->localBounds().size(), QSizeF(200, 50));
        QCOMPARE(r->position(), QPointF(100, 25));
        r->setRotation(90);
        const QRectF b = r->sceneBounds();
        QVERIFY(qAbs(b.height() - 200 - 2 * r->outlineMargin()) < 1.0);
        auto circle = ShapeObject::createBox(ShapeKind::Circle, QRectF(0, 0, 100, 100), ShapeStyle());
        QVERIFY(circle->keepAspectRatio());
        auto arrow = ShapeObject::createLine(ShapeKind::Arrow, QPointF(0, 0), QPointF(100, 0), ShapeStyle());
        QCOMPARE(arrow->controlPoints().size(), 2);
        arrow->moveControlPointTo(1, QPointF(0, 100));
        QCOMPARE(arrow->mapToPage(arrow->controlPoints()[1]), QPointF(0, 100));
        QCOMPARE(arrow->mapToPage(arrow->controlPoints()[0]), QPointF(0, 0));
    }

    void filledShapeHit()
    {
        ShapeStyle filled;
        filled.fill = QColor(255, 0, 0, 80);
        auto f = ShapeObject::createBox(ShapeKind::Ellipse, QRectF(0, 0, 100, 100), filled);
        QVERIFY(f->hitTest(QPointF(50, 50), 0));
        auto hollow = ShapeObject::createBox(ShapeKind::Ellipse, QRectF(0, 0, 100, 100), ShapeStyle());
        QVERIFY(!hollow->hitTest(QPointF(50, 50), 2));
        QVERIFY(hollow->hitTest(QPointF(0, 50), 2));
    }

    void shapeToInk()
    {
        // A rotated, dashed rectangle becomes one closed stroke along its outline.
        ShapeStyle style;
        style.stroke = QColor(255, 0, 0);
        style.width = 6;
        style.dashed = true;
        auto rect = ShapeObject::createBox(ShapeKind::Rectangle, QRectF(100, 100, 200, 100), style);
        rect->setRotation(30);
        auto ink = rect->toInk();
        QCOMPARE(int(ink.size()), 1);
        QCOMPARE(ink[0]->type(), ObjectType::Stroke);
        auto* s = static_cast<StrokeObject*>(ink[0].get());
        QCOMPARE(s->ink().color, style.stroke);
        QCOMPARE(s->ink().width, 6.0);
        QCOMPARE(s->ink().style, StrokeStyle::Dashed);
        QVERIFY(!s->ink().pressure);
        // The ink lies exactly on the rotated outline (corners included) and is closed.
        for (const QPointF& corner : {QPointF(-100, -50), QPointF(100, -50), QPointF(100, 50), QPointF(-100, 50)}) {
            const QPointF page = rect->mapToPage(corner);
            QVERIFY(s->hitTest(page, 0.5));
        }
        QVERIFY(!s->hitTest(rect->mapToPage(QPointF(0, 0)), 0.5)); // hollow
        // An arrow: a straight shaft plus a filled head.
        auto arrow = ShapeObject::createLine(ShapeKind::Arrow, QPointF(0, 0), QPointF(300, 0), ShapeStyle());
        ink = arrow->toInk();
        QCOMPARE(int(ink.size()), 2);
        QCOMPARE(ink[0]->type(), ObjectType::Stroke);
        QCOMPARE(ink[1]->type(), ObjectType::Shape);
        QVERIFY(ink[0]->hitTest(QPointF(150, 0), 0.5));
        QVERIFY(ink[1]->hitTest(QPointF(299, 0), 0.5));
        // A circle keeps its round shape.
        auto circle = ShapeObject::createBox(ShapeKind::Circle, QRectF(0, 0, 200, 200), ShapeStyle());
        ink = circle->toInk();
        QCOMPARE(int(ink.size()), 1);
        for (int deg = 0; deg < 360; deg += 30) {
            const qreal r = qDegreesToRadians(qreal(deg));
            QVERIFY(ink[0]->hitTest(QPointF(100 + 100 * std::cos(r), 100 + 100 * std::sin(r)), 1.0));
        }
    }

    void textWrapsAndScales()
    {
        TextFormat f;
        f.pixelSize = 30;
        auto t = TextObject::create(QStringLiteral("one two three four five six seven"), QPointF(0, 0), f, 150);
        const qreal h1 = t->localBounds().height();
        QVERIFY(h1 > 60); // wrapped onto several lines
        t->resizeTo(QRectF(-t->localBounds().width(), -h1, t->localBounds().width() * 2, h1 * 2), ResizeHint::Corner);
        QCOMPARE(t->format().pixelSize, 60);
        t->resizeTo(QRectF(-300, -10, 600, 20), ResizeHint::Horizontal);
        QCOMPARE(t->format().pixelSize, 60);
        QCOMPARE(t->boxWidth(), 600.0);
    }

    void measurementValues()
    {
        const CoordinateSystem cs = CoordinateSystem::global(QPointF(0, 0), 40, QStringLiteral("cm"));
        auto d = MeasurementObject::create(MeasureKind::Distance, {QPointF(0, 0), QPointF(120, 160)}, Qt::yellow);
        QCOMPARE(d->valueText(cs), QStringLiteral("5 cm"));
        auto a = MeasurementObject::create(MeasureKind::Angle, {QPointF(100, 0), QPointF(0, 0), QPointF(0, -100)}, Qt::yellow);
        QCOMPARE(a->valueText(cs), QStringLiteral("90°"));
        auto s = MeasurementObject::create(MeasureKind::Slope, {QPointF(0, 0), QPointF(40, -80)}, Qt::yellow);
        QCOMPARE(s->valueText(cs), QStringLiteral("m = 2"));
        auto area = MeasurementObject::create(MeasureKind::Area, {QPointF(0, 0), QPointF(80, 0), QPointF(80, 40), QPointF(0, 40)}, Qt::yellow);
        QCOMPARE(area->valueText(cs), QStringLiteral("A = 2 cm²  ·  P = 6 cm"));
        // Editing a point updates the live value.
        d->moveControlPointTo(1, QPointF(0, 40));
        QCOMPARE(d->valueText(cs), QStringLiteral("1 cm"));
    }

    void measurementInsideGraphUsesGraphUnits()
    {
        Page page;
        auto graph = GraphObject::create(QPointF(500, 500), QSizeF(400, 400));
        graph->setWindow(QRectF(-10, -10, 20, 20)); // 20 px per unit
        page.insertObject(0, std::move(graph));
        const CoordinateSystem global = CoordinateSystem::global(QPointF(0, 0), 40, QStringLiteral("cm"));
        const QVector<QPointF> pts{QPointF(500, 500), QPointF(560, 420)};
        const CoordinateSystem* cs = resolveCoordinateSystem(&page, &global, pts);
        QVERIFY(cs != &global);
        QCOMPARE(cs->toMath(QPointF(500, 500)), QPointF(0, 0));
        QVERIFY(qAbs(cs->mathDistance(pts[0], pts[1]) - 5.0) < 1e-9);
        // Outside the graph the page system applies.
        QCOMPARE(resolveCoordinateSystem(&page, &global, {QPointF(0, 0), QPointF(10, 10)}), &global);
    }

    void graphParametersAndView()
    {
        auto g = GraphObject::create(QPointF(0, 0), QSizeF(400, 300));
        const int f = g->addFunction(QStringLiteral("y = a x + b"), QColor());
        QCOMPARE(int(g->parameters().size()), 2);
        g->setParameterValue(QStringLiteral("a"), 2);
        g->setParameterValue(QStringLiteral("b"), -1);
        QCOMPARE(g->evaluate(f, 3), 5.0);
        g->setFunctionExpression(f, QStringLiteral("x^2"));
        QVERIFY(g->parameters().empty());
        g->setFunctionExpression(f, QStringLiteral("x^^2"));
        QVERIFY(!g->functionError(f).isEmpty());
        const QRectF before = g->window();
        g->zoomAtLocal(QPointF(0, 0), 2.0);
        QCOMPARE(g->window().width(), before.width() / 2);
        g->panLocal(QPointF(40, 0));
        QVERIFY(g->window().left() < -before.width() / 4);
        QCOMPARE(g->localToMath(g->mathToLocal(QPointF(1.5, -2))), QPointF(1.5, -2));
    }

    void geometryLabels()
    {
        const CoordinateSystem cs = CoordinateSystem::global(QPointF(0, 0), 40, QStringLiteral("cm"));
        auto p = GeometryObject::create(ConstructKind::Point, {QPointF(80, -120)}, Qt::cyan, true);
        p->setName(QStringLiteral("A"));
        QCOMPARE(p->labelText(cs), QStringLiteral("A (2, 3)"));
        auto v = GeometryObject::create(ConstructKind::Vector, {QPointF(0, 0), QPointF(120, -160)}, Qt::cyan, true);
        QCOMPARE(v->labelText(cs), QStringLiteral("⟨3, 4⟩  |v| = 5"));
        auto l = GeometryObject::create(ConstructKind::Line, {QPointF(0, 40), QPointF(40, 0)}, Qt::cyan, true);
        QCOMPARE(l->labelText(cs), QString(QStringLiteral("y = 1x ") + QChar(0x2212) + QStringLiteral(" 1")));
    }

    void tableCells()
    {
        auto t = TableObject::create(3, 2, QPointF(0, 0));
        t->setCell(1, 1, QStringLiteral("7"));
        t->setDimensions(4, 3);
        QCOMPARE(t->cell(1, 1), QStringLiteral("7"));
        int r = -1, c = -1;
        QVERIFY(t->cellAt(t->cellRect(2, 2).center(), &r, &c));
        QCOMPARE(r, 2);
        QCOMPARE(c, 2);
    }

    void imageStoreDeduplicates()
    {
        ImageStore store;
        QImage img(5000, 100, QImage::Format_RGB32);
        img.fill(Qt::green);
        const QString a = store.addImage(img);
        const QString b = store.addImage(img);
        QCOMPARE(a, b);
        QCOMPARE(store.count(), 1);
        QVERIFY(store.image(a).width() <= ImageStore::kMaxDecodedSide);
        QCOMPARE(store.originalSize(a), QSize(5000, 100));
        const QImage small = store.imageForSize(a, QSizeF(300, 10));
        QVERIFY(small.width() < store.image(a).width());
        QVERIFY(small.width() >= 300);
    }

    void shapeRecognizer()
    {
        ShapeRecognizer rec;
        QVector<QPointF> circle;
        for (int i = 0; i <= 64; ++i) {
            const double a = 2 * geom::kPi * i / 64;
            circle.push_back(QPointF(200 + 100 * std::cos(a) + (i % 3), 200 + 100 * std::sin(a)));
        }
        QCOMPARE(rec.classify(circle, 1.0).kind, ShapeRecognizer::Result::Kind::Circle);

        QVector<QPointF> rect;
        auto edge = [&rect](QPointF a, QPointF b) {
            for (int i = 0; i < 20; ++i)
                rect.push_back(geom::lerp(a, b, i / 20.0));
        };
        edge(QPointF(0, 0), QPointF(300, 0));
        edge(QPointF(300, 0), QPointF(300, 150));
        edge(QPointF(300, 150), QPointF(0, 150));
        edge(QPointF(0, 150), QPointF(0, 2));
        QCOMPARE(rec.classify(rect, 1.0).kind, ShapeRecognizer::Result::Kind::Rectangle);

        QVector<QPointF> tri;
        auto triEdge = [&tri](QPointF a, QPointF b) {
            for (int i = 0; i < 20; ++i)
                tri.push_back(geom::lerp(a, b, i / 20.0));
        };
        triEdge(QPointF(0, 200), QPointF(100, 0));
        triEdge(QPointF(100, 0), QPointF(200, 200));
        triEdge(QPointF(200, 200), QPointF(2, 199));
        const auto t = rec.classify(tri, 1.0);
        QCOMPARE(t.kind, ShapeRecognizer::Result::Kind::Polygon);
        QCOMPARE(t.points.size(), 3);

        QVector<QPointF> line;
        for (int i = 0; i < 30; ++i)
            line.push_back(QPointF(i * 10, 100 + (i % 2)));
        const auto l = rec.classify(line, 1.0);
        QCOMPARE(l.kind, ShapeRecognizer::Result::Kind::Line);
        QVERIFY(qAbs(l.points[1].y() - l.points[0].y()) < 1e-6); // snapped horizontal

        QVector<QPointF> scribble;
        for (int i = 0; i < 60; ++i)
            scribble.push_back(QPointF(i * 5, 100 + 60 * std::sin(i * 0.9)));
        QCOMPARE(rec.classify(scribble, 1.0).kind, ShapeRecognizer::Result::Kind::None);
    }

    void rulerConstraint()
    {
        Ruler ruler;
        ruler.setPosition(QPointF(500, 500));
        ruler.setRotation(30);
        EdgeConstraint c;
        const QPointF nearTopEdge = ruler.toPage(QPointF(100, -48 - 5));
        QVERIFY(ruler.edgeConstraint(nearTopEdge, 10, &c));
        const QPointF projected = c.project(ruler.toPage(QPointF(-200, -60)));
        QVERIFY(qAbs(ruler.toLocal(projected).y() + 48) < 1e-6);
        QVERIFY(!ruler.edgeConstraint(QPointF(500, 500), 10, &c)); // centre of the body
    }

    void compassDrawsArc()
    {
        Compass compass;
        compass.setPosition(QPointF(0, 0));
        compass.setRadius(100);
        QVERIFY(compass.beginInteraction(Compass::kPencilHandle, QPointF(100, 0)));
        for (int deg = 5; deg <= 90; deg += 5)
            compass.updateInteraction(QPointF(100 * std::cos(geom::degToRad(deg)), 100 * std::sin(geom::degToRad(deg))));
        InstrumentResult r = compass.endInteraction();
        QCOMPARE(r.objects.size(), size_t(1));
        auto* stroke = static_cast<StrokeObject*>(r.objects.front().get());
        for (const QPointF& p : stroke->pagePoints())
            QVERIFY(qAbs(geom::length(p) - 100) < 0.5);
    }

    void protractorStamp()
    {
        Protractor p;
        p.setPosition(QPointF(0, 0));
        p.setArmAngles(20, 65);
        QCOMPARE(p.measuredAngle(), 45.0);
        QVERIFY(p.beginInteraction(Protractor::kStampHandle, QPointF(0, 0)));
        InstrumentResult r = p.endInteraction();
        QCOMPARE(r.objects.size(), size_t(1));
        const CoordinateSystem cs;
        QCOMPARE(static_cast<MeasurementObject*>(r.objects.front().get())->valueText(cs), QStringLiteral("45°"));
    }
};

QTEST_MAIN(TestObjects)
#include "tst_objects.moc"
