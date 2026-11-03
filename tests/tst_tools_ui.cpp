// Integration tests for touch gestures, transform handles and the mathematical tools on the
// real canvas. Set CLASSBOARD_SCREENSHOTS=<dir> to store screenshots.
#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "geometry/InstrumentLayer.h"
#include "geometry/Instruments.h"
#include "geometry/MeasurementObject.h"
#include "document/ImageObject.h"
#include "math/equation/EquationObject.h"
#include "graph/TableObject.h"
#include "tools/EditOperations.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QClipboard>
#include <QLineEdit>
#include <QTouchEvent>
#include <QtTest>

using namespace cb;

class TestToolsUi : public QObject
{
    Q_OBJECT
private:
    std::unique_ptr<UiContext> m_ui;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<MainWindow> m_window;
    QTouchDevice* m_touch = nullptr;
    QString m_shots;

    CanvasWidget& canvas() { return m_window->canvas(); }
    Document& doc() { return m_window->document(); }
    Page& page() { return *doc().currentPage(); }
    QPoint toView(const QPointF& p) { return canvas().view().pageToView(p).toPoint(); }
    QPointF toPage(const QPoint& p) { return canvas().view().viewToPage(QPointF(p)); }

    /// Three fingers held together (about 16 mm apart) wiping straight down from y0 to y0 + 96.
    void threeFingerWipe(int x0, int y0)
    {
        QTest::touchEvent(&canvas(), m_touch)
            .press(0, QPoint(x0, y0 + 10), &canvas())
            .press(1, QPoint(x0 + 30, y0), &canvas())
            .press(2, QPoint(x0 + 60, y0 + 5), &canvas());
        for (int i = 1; i <= 8; ++i)
            QTest::touchEvent(&canvas(), m_touch)
                .move(0, QPoint(x0, y0 + 10 + i * 12), &canvas())
                .move(1, QPoint(x0 + 30, y0 + i * 12), &canvas())
                .move(2, QPoint(x0 + 60, y0 + 5 + i * 12), &canvas());
        QTest::touchEvent(&canvas(), m_touch)
            .release(0, QPoint(x0, y0 + 106), &canvas())
            .release(1, QPoint(x0 + 30, y0 + 96), &canvas())
            .release(2, QPoint(x0 + 60, y0 + 101), &canvas());
    }

    void mouseDrag(const QPoint& from, const QPoint& to, int steps = 12)
    {
        QTest::mousePress(&canvas(), Qt::LeftButton, Qt::NoModifier, from);
        for (int i = 1; i <= steps; ++i) {
            const QPoint p = from + (to - from) * i / steps;
            QMouseEvent move(QEvent::MouseMove, p, canvas().mapToGlobal(p), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&canvas(), &move);
        }
        QTest::mouseRelease(&canvas(), Qt::LeftButton, Qt::NoModifier, to);
    }

    void shot(const QString& name)
    {
        if (m_shots.isEmpty())
            return;
        QTest::qWait(200);
        m_window->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    void clearPage()
    {
        std::vector<ObjectId> ids;
        for (const auto& o : page().objects())
            ids.push_back(o->id());
        if (!ids.empty())
            doc().commands().push(std::make_unique<RemoveObjectsCommand>(page().id(), std::move(ids)));
        canvas().selectionModel().clear();
    }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(classboard);
        QStandardPaths::setTestModeEnabled(true);
        m_shots = qEnvironmentVariable("CLASSBOARD_SCREENSHOTS");
        m_touch = QTest::createTouchDevice();
        m_ui = std::make_unique<UiContext>();
        QVERIFY(m_ui->theme.load(QStringLiteral(":/themes/dark.json")));
        m_settings = std::make_unique<AppSettings>();
        m_settings->setUiScale(1.0);
        m_settings->setMultiUserTouch(false);
        m_settings->setPalmErase(true);
        m_window = std::make_unique<MainWindow>(*m_ui, *m_settings);
        m_window->resize(1600, 1000);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_window->startup(QString());
        m_window->resize(1600, 1000);
        QTest::qWait(100);
        canvas().fitPage();
    }

    void cleanupTestCase()
    {
        doc().markSaved();
        m_window.reset();
    }

    void touchDrawing()
    {
        clearPage();
        m_window->activateTool(ToolId::Pen);
        QTest::touchEvent(&canvas(), m_touch).press(0, QPoint(200, 200), &canvas());
        for (int i = 1; i <= 10; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, QPoint(200 + i * 30, 200 + i * 5), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, QPoint(500, 250), &canvas());
        QCOMPARE(page().objectCount(), 1);
    }

    void pinchZooms()
    {
        const qreal before = canvas().zoom();
        QTest::touchEvent(&canvas(), m_touch).press(0, QPoint(700, 400), &canvas()).press(1, QPoint(800, 400), &canvas());
        for (int i = 1; i <= 10; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, QPoint(700 - i * 10, 400), &canvas()).move(1, QPoint(800 + i * 10, 400), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, QPoint(600, 400), &canvas()).release(1, QPoint(900, 400), &canvas());
        QVERIFY2(canvas().zoom() > before * 2.5, qPrintable(QString::number(canvas().zoom() / before)));
        QCOMPARE(page().objectCount(), 1); // the pinch did not draw
        canvas().fitPage();
    }

    void palmEraseWipes()
    {
        clearPage();
        m_window->activateTool(ToolId::Pen);
        for (int y = 300; y <= 420; y += 40)
            mouseDrag(QPoint(300, y), QPoint(900, y));
        QCOMPARE(page().objectCount(), 4);
        const int commandsBefore = doc().commands().index();
        // Three fingers land together and wipe across the middle of the lines.
        threeFingerWipe(560, 320);
        // Every line was cut in two, the fingers left no ink, and the whole wipe is one undo step.
        QCOMPARE(page().objectCount(), 8);
        QCOMPARE(doc().commands().index(), commandsBefore + 1);
        doc().commands().undo();
        QCOMPARE(page().objectCount(), 4);
        doc().commands().redo();
        QCOMPARE(page().objectCount(), 8);
        doc().commands().undo();
    }

    void wipeErasesShapesAndEquations()
    {
        clearPage();
        m_window->activateTool(ToolId::Select); // the wipe works whatever tool is active
        const QPointF a = toPage(QPoint(590, 330));
        const QPointF b = toPage(QPoint(590, 430));
        const qreal mm = (b.y() - a.y()) / 100.0 * 4.0; // a few view pixels in page units
        std::vector<ObjectPtr> objs;
        std::vector<ObjectId> wiped;
        auto add = [&](ObjectPtr o, bool expectWiped) {
            if (expectWiped)
                wiped.push_back(o->id());
            objs.push_back(std::move(o));
        };
        ShapeStyle style;
        // Outline shapes whose edges cross the wipe, a line, an arrow and an equation under it.
        add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(QPointF(a.x(), a.y() + 5 * mm), QPointF(a.x() + 60 * mm, b.y())), style), true);
        add(ShapeObject::createBox(ShapeKind::Circle, QRectF(QPointF(a.x() - 50 * mm, a.y() + 4 * mm), QSizeF(50 * mm, 50 * mm)), style), true);
        add(ShapeObject::createBox(ShapeKind::Triangle, QRectF(QPointF(a.x() - 10 * mm, a.y() + 2 * mm), QSizeF(20 * mm, 15 * mm)), style), true);
        add(ShapeObject::createLine(ShapeKind::Line, QPointF(a.x() - 40 * mm, (a.y() + b.y()) / 2), QPointF(a.x() + 40 * mm, (a.y() + b.y()) / 2), style), true);
        add(ShapeObject::createLine(ShapeKind::Arrow, QPointF(a.x() - 40 * mm, b.y() - 6 * mm), QPointF(a.x() + 40 * mm, b.y() - 6 * mm), style), true);
        add(EquationObject::create(QStringLiteral("x^2+1"), (a + b) / 2, 28.0, Qt::white), true);
        // Far away: untouched. A picture / imported slide under the wipe: untouched.
        add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(toPage(QPoint(100, 100)), QSizeF(20 * mm, 20 * mm)), style), false);
        add(ImageObject::create(QStringLiteral("missing-image"), QSizeF(80 * mm, 80 * mm), (a + b) / 2), false);
        const int total = static_cast<int>(objs.size());
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        QCOMPARE(page().objectCount(), total);

        const int commandsBefore = doc().commands().index();
        threeFingerWipe(560, 320);
        for (const ObjectId& id : wiped)
            QVERIFY2(!page().object(id), "an object under the wipe survived");
        // Shapes are cut like ink: what lay under the wipe is gone, the rest stays as ink.
        const QPointF pathMid = (a + b) / 2;
        for (const auto& o : page().objects()) {
            if (o->type() == ObjectType::Image)
                continue;
            QVERIFY2(!o->hitTest(pathMid, 0.0), "something is left in the middle of the wipe");
        }
        int strokes = 0;
        for (const auto& o : page().objects())
            strokes += o->type() == ObjectType::Stroke;
        QVERIFY2(strokes >= 4, "the parts of the shapes outside the wipe should remain");
        QCOMPARE(doc().commands().index(), commandsBefore + 1);
        // Undo brings everything back, redo wipes again.
        const int afterWipe = page().objectCount();
        doc().commands().undo();
        QCOMPARE(page().objectCount(), total);
        for (const ObjectId& id : wiped)
            QVERIFY(page().object(id));
        doc().commands().redo();
        QCOMPARE(page().objectCount(), afterWipe);
        doc().commands().undo();
        m_window->activateTool(ToolId::Pen);
    }

    // Anything (other than the given ids) drawn at a page point?
    bool inkAt(const QPointF& pagePos, qreal tolerance = 1.0)
    {
        for (const auto& o : page().objects())
            if (o->hitTest(pagePos, tolerance))
                return true;
        return false;
    }

    void areaEraserCutsPartOfAShape()
    {
        clearPage();
        m_window->toolSettings().setEraserMode(EraserMode::Area);
        m_window->activateTool(ToolId::Eraser);
        // A triangle: apex top centre, base along the bottom.
        const QRectF box(toPage(QPoint(300, 200)), toPage(QPoint(700, 500)));
        auto tri = ShapeObject::createBox(ShapeKind::Triangle, box, ShapeStyle());
        const ObjectId id = tri->id();
        std::vector<ObjectPtr> objs;
        objs.push_back(std::move(tri));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        const QPointF baseMid(box.center().x(), box.bottom());
        const QPointF leftSideMid((box.left() + box.center().x()) / 2, box.center().y());
        const QPointF rightSideMid((box.right() + box.center().x()) / 2, box.center().y());
        QVERIFY(inkAt(baseMid) && inkAt(leftSideMid) && inkAt(rightSideMid));

        // Rub out the middle of the base only.
        const int commandsBefore = doc().commands().index();
        mouseDrag(QPoint(440, 500), QPoint(560, 500));
        QVERIFY(!page().object(id));                       // no longer a whole triangle ...
        QVERIFY(!inkAt(baseMid));                          // ... the base has a gap ...
        QVERIFY(inkAt(leftSideMid) && inkAt(rightSideMid)); // ... and both sides are still there
        QVERIFY(inkAt(QPointF(box.left() + 2, box.bottom()))); // as are the ends of the base
        for (const auto& o : page().objects())
            QCOMPARE(o->type(), ObjectType::Stroke);
        QCOMPARE(doc().commands().index(), commandsBefore + 1); // one undo step
        shot(QStringLiteral("erase-part-of-triangle"));

        // Rub out the whole left side as well: only the right side and the base ends remain.
        mouseDrag(QPoint(300, 500), QPoint(500, 200), 30);
        QVERIFY(!inkAt(leftSideMid));
        QVERIFY(inkAt(rightSideMid));

        // Undo restores the real triangle, redo erases again.
        doc().commands().undo();
        doc().commands().undo();
        QCOMPARE(page().objectCount(), 1);
        QVERIFY(page().object(id));
        QCOMPARE(page().object(id)->type(), ObjectType::Shape);
        doc().commands().redo();
        QVERIFY(!page().object(id));
        QVERIFY(!inkAt(baseMid));
        QVERIFY(inkAt(leftSideMid));
        m_window->activateTool(ToolId::Pen);
    }

    void areaEraserCutsCirclesLinesAndArrows()
    {
        clearPage();
        m_window->toolSettings().setEraserMode(EraserMode::Area);
        m_window->activateTool(ToolId::Eraser);
        ShapeStyle style;
        style.fill = QColor(0, 120, 255, 80); // filled: the fill goes, the outline is cut
        auto circle = ShapeObject::createBox(ShapeKind::Circle, QRectF(toPage(QPoint(150, 150)), toPage(QPoint(350, 350))), style);
        const QRectF cb = circle->sceneBounds();
        auto arrow = ShapeObject::createLine(ShapeKind::Arrow, toPage(QPoint(420, 250)), toPage(QPoint(720, 250)), ShapeStyle());
        std::vector<ObjectPtr> objs;
        objs.push_back(std::move(circle));
        objs.push_back(std::move(arrow));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        // A gap at the top of the circle and in the middle of the arrow's shaft.
        mouseDrag(QPoint(230, 150), QPoint(270, 150), 6);
        mouseDrag(QPoint(560, 230), QPoint(560, 270), 6);
        QVERIFY(!inkAt(toPage(QPoint(250, 150))));
        QVERIFY(inkAt(toPage(QPoint(250, 350)), 3.0)); // bottom of the circle stays
        QVERIFY(inkAt(toPage(QPoint(150, 250)), 3.0)); // and its sides
        QVERIFY(!inkAt(toPage(QPoint(560, 250))));
        QVERIFY(inkAt(toPage(QPoint(470, 250))));      // both halves of the shaft
        QVERIFY(inkAt(toPage(QPoint(650, 250))));
        QVERIFY(inkAt(toPage(QPoint(716, 250))));      // and the arrowhead
        QVERIFY(!inkAt(cb.center()));                  // the fill is gone
        int shapes = 0;
        for (const auto& o : page().objects())
            shapes += o->type() == ObjectType::Shape;
        QCOMPARE(shapes, 1); // only the arrowhead is a (small, filled) shape
        // Object mode still removes a whole shape in one tap.
        clearPage();
        auto rect = ShapeObject::createBox(ShapeKind::Rectangle, QRectF(toPage(QPoint(300, 200)), toPage(QPoint(600, 400))), ShapeStyle());
        objs.clear();
        objs.push_back(std::move(rect));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        m_window->toolSettings().setEraserMode(EraserMode::Object);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, QPoint(300, 300));
        QCOMPARE(page().objectCount(), 0);
        m_window->toolSettings().setEraserMode(EraserMode::Area);
        m_window->activateTool(ToolId::Pen);
    }

    void touchDensityFollowsTheScreen()
    {
        // 65" 4K board at 200 % Windows scaling: 1920 x 1080 logical pixels on 1430 x 804 mm.
        const qreal board = CanvasWidget::pixelsPerMm(QSize(1920, 1080), QSizeF(1430, 804));
        QVERIFY(std::abs(board - 1.343) < 0.01);
        // The same board at 100 %: twice as many logical pixels per millimetre.
        QVERIFY(std::abs(CanvasWidget::pixelsPerMm(QSize(3840, 2160), QSizeF(1430, 804)) - 2 * board) < 0.02);
        // Missing or nonsensical physical size: 96 dpi.
        QVERIFY(std::abs(CanvasWidget::pixelsPerMm(QSize(1920, 1080), QSizeF(0, 0)) - 96.0 / 25.4) < 1e-9);
        QVERIFY(std::abs(CanvasWidget::pixelsPerMm(QSize(3840, 2160), QSizeF(160, 90)) - 96.0 / 25.4) < 1e-9);
    }

    void tenPointTouchByDefault()
    {
        clearPage();
        m_window->activateTool(ToolId::Pen);
        QVERIFY(!canvas().input().multiUserTouch());
        const qreal ppm = canvas().input().pixelsPerMm();
        canvas().input().setPixelsPerMm(1.0); // fingers 180 px = 180 mm apart: separate people
        const qreal zoom = canvas().zoom();
        // Four students start one after the other (people never land in the same instant).
        for (int f = 0; f < 4; ++f) {
            QTest::touchEvent(&canvas(), m_touch).press(f, QPoint(80 + f * 180, 150), &canvas());
            QTest::qWait(260);
        }
        for (int i = 1; i <= 8; ++i) {
            auto move = QTest::touchEvent(&canvas(), m_touch);
            for (int f = 0; f < 4; ++f)
                move.move(f, QPoint(80 + f * 180 + i * 6, 150 + i * 30), &canvas());
        }
        auto release = QTest::touchEvent(&canvas(), m_touch);
        for (int f = 0; f < 4; ++f)
            release.release(f, QPoint(80 + f * 180 + 48, 390), &canvas());
        release.commit();
        // Four independent lines, one undo step each, and the view did not move.
        QCOMPARE(page().objectCount(), 4);
        QCOMPARE(canvas().zoom(), zoom);
        // Two fingers of one hand still zoom on an otherwise idle board.
        canvas().input().setPixelsPerMm(ppm);
        QTest::touchEvent(&canvas(), m_touch).press(0, QPoint(400, 300), &canvas()).press(1, QPoint(460, 300), &canvas());
        for (int i = 1; i <= 8; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, QPoint(400 - i * 10, 300), &canvas()).move(1, QPoint(460 + i * 10, 300), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, QPoint(320, 300), &canvas()).release(1, QPoint(540, 300), &canvas());
        QVERIFY(canvas().zoom() > zoom * 2.0);
        QCOMPARE(page().objectCount(), 4);
        canvas().fitPage();
    }

    void separatedFingersWriteInMultiUserMode()
    {
        clearPage();
        m_window->activateTool(ToolId::Pen);
        canvas().input().setMultiUserTouch(true);
        // Three students write at once: three independent strokes.
        QTest::touchEvent(&canvas(), m_touch)
            .press(0, QPoint(150, 250), &canvas())
            .press(1, QPoint(400, 250), &canvas())
            .press(2, QPoint(650, 250), &canvas());
        for (int i = 1; i <= 8; ++i)
            QTest::touchEvent(&canvas(), m_touch)
                .move(0, QPoint(150 + i * 5, 250 + i * 12), &canvas())
                .move(1, QPoint(400 - i * 5, 250 + i * 12), &canvas())
                .move(2, QPoint(650 + i * 3, 250 + i * 12), &canvas());
        QTest::touchEvent(&canvas(), m_touch)
            .release(0, QPoint(190, 346), &canvas())
            .release(1, QPoint(360, 346), &canvas())
            .release(2, QPoint(674, 346), &canvas());
        QCOMPARE(page().objectCount(), 3);
        std::vector<ObjectId> strokes;
        for (const auto& o : page().objects())
            strokes.push_back(o->id());
        // Three fingers held together still wipe in this mode: the middle stroke is erased where
        // the wipe passed, the others stay, and the fingers add no ink (a single "Erase" step).
        const int commandsBefore = doc().commands().index();
        threeFingerWipe(370, 230);
        QCOMPARE(doc().commands().index(), commandsBefore + 1);
        int survivors = 0;
        for (const ObjectId& id : strokes)
            survivors += page().object(id) != nullptr;
        QCOMPARE(survivors, 2);
        for (const auto& o : page().objects())
            QCOMPARE(o->type(), ObjectType::Stroke);
        canvas().input().setMultiUserTouch(false);
    }

    void resizeAndRotateHandles()
    {
        clearPage();
        auto shape = ShapeObject::createBox(ShapeKind::Rectangle, QRectF(600, 300, 400, 200), ShapeStyle());
        const ObjectId id = shape->id();
        std::vector<ObjectPtr> objs;
        objs.push_back(std::move(shape));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        m_window->activateTool(ToolId::Select);
        canvas().selectionModel().setSingle(id);
        QTest::qWait(20);

        // Drag the bottom-right handle outwards.
        const QPoint br = toView(QPointF(1000, 500));
        mouseDrag(br, br + QPoint(80, 40));
        DocumentObject* o = page().object(id);
        QVERIFY(o->localBounds().width() > 450);
        QVERIFY(o->localBounds().height() > 220);
        // The opposite corner stays in place.
        QVERIFY(geom::distance(o->mapToPage(o->localBounds().topLeft()), QPointF(600, 300)) < 1.0);

        // Drag the rotation handle (above the top edge) to the right.
        const QRectF b = o->sceneBounds();
        const QPointF topCenterView = canvas().view().pageToView(QPointF(o->mapToPage(QPointF(0, o->localBounds().top()))));
        const QPoint rot = (topCenterView + QPointF(0, -40)).toPoint();
        const QPoint centerView = toView(b.center());
        mouseDrag(rot, QPoint(centerView.x() + 300, centerView.y()), 20);
        QVERIFY2(qAbs(page().object(id)->rotation() - 90.0) < 0.5, qPrintable(QString::number(page().object(id)->rotation())));
        shot(QStringLiteral("10-rotated-selection"));
        doc().commands().undo();
        QCOMPARE(page().object(id)->rotation(), 0.0);
    }

    void measureDistanceSnapsToGrid()
    {
        clearPage();
        TemplateSpec grid;
        grid.id = QStringLiteral("grid");
        grid.kind = TemplateKind::Grid;
        doc().commands().push(std::make_unique<ModifyPageCommand>(page().id(), QString(), grid, QStringLiteral("grid")));
        m_window->toolSettings().setSnapToGrid(true);
        m_window->toolSettings().setMeasureKind(MeasureKind::Distance);
        m_window->activateTool(ToolId::Measure);
        // Origin (960, 540); 40 px per unit. Drag from ~(0,0) to ~(3,4) with a small offset.
        mouseDrag(toView(QPointF(963, 537)), toView(QPointF(1082, 382)));
        QCOMPARE(page().objectCount(), 1);
        auto* m = static_cast<MeasurementObject*>(page().objectAt(0));
        QCOMPARE(m->valueText(doc().coordinates()), QStringLiteral("5 cm"));
        shot(QStringLiteral("11-measure"));
    }

    void constructPointShowsCoordinates()
    {
        m_window->toolSettings().setConstructKind(ConstructKind::Point);
        m_window->activateTool(ToolId::Construct);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(QPointF(1041, 459)));
        QCOMPARE(page().objectAt(page().objectCount() - 1)->type(), ObjectType::Geometry);
        const QPointF p = page().objectAt(page().objectCount() - 1)->mapToPage(page().objectAt(page().objectCount() - 1)->controlPoints()[0]);
        QCOMPARE(doc().coordinates().toMath(p), QPointF(2, 2));
    }

    void compassDrawsCircle()
    {
        clearPage();
        InstrumentLayer& layer = canvas().instruments();
        layer.show(Instrument::Kind::Compass, QPointF(700, 540));
        auto* compass = static_cast<Compass*>(layer.find(Instrument::Kind::Compass));
        QVERIFY(compass);
        const QPointF pencil = compass->toPage(QPointF(compass->radius(), 0));
        QTest::mousePress(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(pencil));
        for (int deg = 10; deg <= 370; deg += 10) {
            const QPointF p = QPointF(700, 540) + geom::rotated(QPointF(compass->radius(), 0), deg);
            QMouseEvent move(QEvent::MouseMove, toView(p), canvas().mapToGlobal(toView(p)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&canvas(), &move);
        }
        QTest::mouseRelease(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(pencil));
        QCOMPARE(page().objectCount(), 1);
        auto* arc = static_cast<StrokeObject*>(page().objectAt(0));
        const QRectF b = arc->sceneBounds();
        QVERIFY(qAbs(b.width() - 2 * compass->radius()) < arc->outlineMargin() * 2 + 2);
        layer.show(Instrument::Kind::Protractor, QPointF(1300, 700));
        layer.show(Instrument::Kind::SetSquare, QPointF(1250, 300));
        shot(QStringLiteral("12-instruments"));
        layer.hide(Instrument::Kind::Compass);
        layer.hide(Instrument::Kind::Protractor);
        layer.hide(Instrument::Kind::SetSquare);
        QVERIFY(layer.isEmpty());
    }

    void instrumentCloseButton()
    {
        InstrumentLayer& layer = canvas().instruments();
        layer.show(Instrument::Kind::Ruler, QPointF(960, 540));
        Instrument* ruler = layer.find(Instrument::Kind::Ruler);
        // The close button sits at the left end of the ruler.
        const QPointF close = ruler->toPage(QPointF(-(30 * 40 + 36) / 2.0 + 34, 20));
        m_window->activateTool(ToolId::Pen);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(close));
        QVERIFY(layer.isEmpty());
    }

    void tableCellEditing()
    {
        clearPage();
        auto table = TableObject::create(3, 3, QPointF(960, 540));
        const ObjectId id = table->id();
        const QPointF cell = table->mapToPage(table->cellRect(1, 1).center());
        std::vector<ObjectPtr> objs;
        objs.push_back(std::move(table));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(objs)));
        m_window->activateTool(ToolId::Select);
        // Double tap: the first tap selects, the second opens the cell editor.
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(cell));
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, toView(cell));
        QLineEdit* editor = nullptr;
        for (QLineEdit* e : canvas().findChildren<QLineEdit*>())
            if (e->isVisible())
                editor = e;
        QVERIFY(editor);
        QTest::keyClicks(editor, QStringLiteral("42"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::qWait(20);
        QCOMPARE(static_cast<TableObject*>(page().object(id))->cell(1, 1), QStringLiteral("42"));
        shot(QStringLiteral("13-table"));
        for (QLineEdit* e : canvas().findChildren<QLineEdit*>())
            if (e->isVisible())
                QTest::keyClick(e, Qt::Key_Escape);
    }

    void copyPasteAndDuplicate()
    {
        clearPage();
        m_window->activateTool(ToolId::Pen);
        mouseDrag(QPoint(300, 300), QPoint(500, 350));
        m_window->activateTool(ToolId::Select);
        EditOperations edit(doc(), canvas().selectionModel());
        edit.selectAll();
        edit.copySelection();
        QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(QString::fromLatin1(EditOperations::kMimeType)));
        QVERIFY(edit.paste(canvas().viewCenterInPage()));
        QCOMPARE(page().objectCount(), 2);
        QVERIFY(page().objectAt(0)->id() != page().objectAt(1)->id());
        edit.duplicateSelection();
        QCOMPARE(page().objectCount(), 3);
        edit.setSelectionColor(QColor(255, 0, 0));
        QCOMPARE(page().objectAt(2)->color(), QColor(255, 0, 0));
        edit.sendToBack();
        QCOMPARE(page().objectAt(0)->color(), QColor(255, 0, 0));
    }

    void doubleTapEditsEquation()
    {
        clearPage();
        m_window->popovers().open(QStringLiteral("equation"), m_window->ribbon().moreButton());
        QTest::qWait(50);
        auto* input = m_window->popoverHost().current()->findChild<QLineEdit*>();
        input->setText(QStringLiteral("\\sum_{k=1}^{n} k^2"));
        QTest::keyClick(input, Qt::Key_Return);
        QTest::qWait(200);
        QCOMPARE(page().objectCount(), 1);
        const QPoint at = toView(page().objectAt(0)->sceneBounds().center());
        m_window->activateTool(ToolId::Select);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, at);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, at);
        QTest::qWait(100);
        QCOMPARE(m_window->popoverHost().currentKey(), QStringLiteral("equation"));
        input = m_window->popoverHost().current()->findChild<QLineEdit*>();
        QCOMPARE(input->text(), QStringLiteral("\\sum_{k=1}^{n} k^2"));
        shot(QStringLiteral("14-equation-edit"));
        m_window->popovers().close();
        QTest::qWait(150);
    }

    void pageNavigatorAndTemplates()
    {
        doc().commands().push(std::make_unique<InsertPageCommand>(1, doc().createPage()));
        doc().commands().push(std::make_unique<InsertPageCommand>(2, doc().createPage()));
        QCOMPARE(doc().currentPageIndex(), 2);
        m_window->popovers().toggle(QStringLiteral("pages"), m_window->ribbon().pageButton());
        QTest::qWait(300);
        shot(QStringLiteral("15-page-navigator"));
        QVERIFY(m_window->popoverHost().isOpen());
        m_window->popovers().close();
        QTest::qWait(150);
        QCOMPARE(doc().pageCount(), 3);
    }
};

QTEST_MAIN(TestToolsUi)
#include "tst_tools_ui.moc"
