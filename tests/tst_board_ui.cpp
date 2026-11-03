// End-to-end tests on the real window for the classroom workflows: grouped ribbon, page menu
// (clear / delete), select-and-move for every object kind (mouse and touch), zoom presets and
// zoom-independent export, exact values in the property panel, and the Magic Equation Maker.
// Set CLASSBOARD_SCREENSHOTS=<dir> to store screenshots.
#include "SyntheticInk.h"

#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ImageObject.h"
#include "document/ObjectFactory.h"
#include "document/PageOperations.h"
#include "document/PageSize.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "export/Exporters.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "geometry/Precision.h"
#include "graph/GraphObject.h"
#include "math/equation/EquationObject.h"
#include "storage/PageImport.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/SelectionBar.h"
#include "ui/UiContext.h"
#include "ui/popovers/MagicEquationPanel.h"
#include "ui/widgets/NumberField.h"
#include "ui/widgets/TouchButton.h"

#include <QBuffer>
#include <QLineEdit>
#include <QTouchEvent>
#include <QtTest>

using namespace cb;

class TestBoardUi : public QObject
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
    QString popoverKey() { return m_window->popoverHost().currentKey(); }

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

    void touchDrag(const QPoint& from, const QPoint& to, int steps = 12)
    {
        QTest::touchEvent(&canvas(), m_touch).press(0, from, &canvas());
        for (int i = 1; i <= steps; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, from + (to - from) * i / steps, &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, to, &canvas());
    }

    void shot(const QString& name)
    {
        if (m_shots.isEmpty())
            return;
        QTest::qWait(250);
        m_window->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    void resetPage()
    {
        m_window->popovers().close();
        QTest::qWait(150);
        while (doc().pageCount() > 1)
            pageops::deletePage(doc(), doc().pageCount() - 1);
        doc().setCurrentPageIndex(0);
        pageops::clearPage(doc(), 0);
        canvas().selectionModel().clear();
        canvas().fitPage();
    }

    template <typename T>
    ObjectId add(std::unique_ptr<T> object)
    {
        const ObjectId id = object->id();
        std::vector<ObjectPtr> v;
        v.push_back(std::move(object));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(v)));
        return id;
    }

    ObjectId addStrokes(const testink::Strokes& ink)
    {
        std::vector<ObjectPtr> v;
        QVector<ObjectId> ids;
        InkStyle style;
        style.width = 4;
        for (const auto& stroke : ink) {
            QVector<StrokePoint> pts;
            for (const QPointF& p : stroke)
                pts.push_back({p, 1.0f});
            auto s = StrokeObject::fromPagePoints(pts, style);
            ids.push_back(s->id());
            v.push_back(std::move(s));
        }
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(v)));
        canvas().selectionModel().clear();
        for (const ObjectId& id : ids)
            canvas().selectionModel().add(id);
        return ids.first();
    }

    Popover* openedPopover()
    {
        QTest::qWait(250);
        return m_window->popoverHost().current();
    }

    NumberField* field(const QString& label)
    {
        Popover* p = m_window->popoverHost().current();
        if (!p)
            return nullptr;
        for (NumberField* f : p->findChildren<NumberField*>())
            if (f->labelText() == label)
                return f;
        return nullptr;
    }

    /// Types a value into an exact-value field as the teacher would and confirms with Enter.
    void enter(const QString& label, const QString& value)
    {
        NumberField* f = field(label);
        QVERIFY2(f, qPrintable(label));
        QLineEdit* edit = f->lineEdit();
        edit->setFocus();
        edit->selectAll();
        QTest::keyClicks(edit, value);
        QTest::keyClick(edit, Qt::Key_Return);
        QTest::qWait(50);
    }

    TouchButton* buttonIn(QWidget* root, const QString& text)
    {
        for (TouchButton* b : root->findChildren<TouchButton*>())
            if (b->text() == text && b->isVisible())
                return b;
        return nullptr;
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
        m_window->resize(1920, 1080);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_window->startup(QString());
        m_window->resize(1920, 1080);
        QTest::qWait(100);
        canvas().fitPage();
    }

    void cleanupTestCase()
    {
        doc().markSaved();
        m_window.reset();
    }

    // ------------------------------------------------------------------------------ ribbon

    void ribbonGroupsOpenTheirPopovers()
    {
        resetPage();
        Ribbon& r = m_window->ribbon();
        const QVector<QPair<TouchButton*, QString>> groups = {{r.fileButton(), QStringLiteral("lesson")},
                                                              {r.insertButton(), QStringLiteral("insert")},
                                                              {r.editButton(), QStringLiteral("edit")},
                                                              {r.pageMenuButton(), QStringLiteral("pageactions")},
                                                              {r.zoomButton(), QStringLiteral("view")}};
        for (const auto& g : groups) {
            QVERIFY2(g.first->isVisible(), qPrintable(g.second));
            QTest::mouseClick(g.first, Qt::LeftButton);
            QCOMPARE(popoverKey(), g.second);
            shot(QStringLiteral("board-ribbon-") + g.second);
            m_window->popovers().close();
            QTest::qWait(150);
        }
        QVERIFY(r.zoomInButton()->isVisible());
        QVERIFY(r.zoomOutButton()->isVisible());
        QVERIFY(r.fitButton()->isVisible());
        shot(QStringLiteral("board-ribbon"));
    }

    void narrowWindowHidesOptionalGroupsOnly()
    {
        m_window->resize(1000, 700);
        QTest::qWait(150);
        Ribbon& r = m_window->ribbon();
        QVERIFY(!r.fitButton()->isVisible());
        for (TouchButton* core : {r.moreButton(), r.penButton(), r.eraserButton(), r.selectButton(), r.pageButton(),
                                  r.newPageButton(), r.pageMenuButton(), r.zoomButton()})
            QVERIFY(core->isVisible());
        // Nothing overlaps: the buttons fit inside the ribbon.
        QVERIFY(r.zoomButton()->geometry().right() <= r.width());
        shot(QStringLiteral("board-ribbon-narrow"));
        m_window->resize(1920, 1080);
        QTest::qWait(150);
        QVERIFY(r.fitButton()->isVisible());
        canvas().fitPage();
    }

    // -------------------------------------------------------------------------- page menu

    void pageMenuClearsAndDeletesPages()
    {
        resetPage();
        pageops::newPage(doc(), 0);
        QCOMPARE(doc().currentPageIndex(), 1);
        add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(300, 300, 400, 200), ShapeStyle()));
        add(ObjectFactory::createText(QStringLiteral("Keep the page"), QPointF(900, 500)));
        const PageId id = page().id();

        QTest::mouseClick(m_window->ribbon().pageMenuButton(), Qt::LeftButton);
        Popover* p = openedPopover();
        QVERIFY(p);
        shot(QStringLiteral("board-page-menu"));
        auto* clear = p->findChild<TouchButton*>(QStringLiteral("clearPage"));
        auto* del = p->findChild<TouchButton*>(QStringLiteral("deletePage"));
        QVERIFY(clear && del);
        QVERIFY(clear->isEnabled() && del->isEnabled());
        QTest::mouseClick(clear, Qt::LeftButton);
        QTest::qWait(150);
        // Confirmation inside the board (no system dialog).
        TouchButton* confirm = buttonIn(m_window.get(), QStringLiteral("Clear page"));
        QVERIFY(confirm);
        shot(QStringLiteral("board-clear-confirm"));
        QTest::mouseClick(confirm, Qt::LeftButton);
        QTest::qWait(100);
        QCOMPARE(doc().pageCount(), 2);
        QCOMPARE(page().id(), id);
        QCOMPARE(page().objectCount(), 0);

        // Delete page removes the page itself; undo restores it.
        QTest::mouseClick(m_window->ribbon().pageMenuButton(), Qt::LeftButton);
        p = openedPopover();
        QTest::mouseClick(p->findChild<TouchButton*>(QStringLiteral("deletePage")), Qt::LeftButton);
        QTest::qWait(100);
        QCOMPARE(doc().pageCount(), 1);
        QCOMPARE(doc().indexOfPage(id), -1);
        doc().commands().undo();
        QCOMPARE(doc().pageCount(), 2);
        QVERIFY(doc().indexOfPage(id) >= 0);
        // The last page cannot be deleted: the button is disabled.
        resetPage();
        QTest::mouseClick(m_window->ribbon().pageMenuButton(), Qt::LeftButton);
        p = openedPopover();
        QVERIFY(!p->findChild<TouchButton*>(QStringLiteral("deletePage"))->isEnabled());
        m_window->popovers().close();
    }

    void pageSizeAndBackgroundThroughPanels()
    {
        resetPage();
        const QSizeF a4 = pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait);
        QVERIFY(pageops::setPageSize(doc(), {0}, a4));
        canvas().fitPage();
        m_window->popovers().open(QStringLiteral("pagesize"), m_window->ribbon().pageMenuButton());
        QVERIFY(openedPopover());
        shot(QStringLiteral("board-page-size"));
        m_window->popovers().open(QStringLiteral("background"), m_window->ribbon().pageMenuButton());
        Popover* p = openedPopover();
        TouchButton* white = buttonIn(p, QStringLiteral("White"));
        QVERIFY(white);
        QTest::mouseClick(white, Qt::LeftButton);
        QCOMPARE(page().background().background, QColor(Qt::white));
        QTest::qWait(100);
        shot(QStringLiteral("board-a4-white"));
        m_window->popovers().close();
        QTest::qWait(150);
        // The A4 page is shown whole after "fit".
        const QRect frame = canvas().pageRectToWidget(page().frameRect());
        QVERIFY(canvas().rect().adjusted(-2, -2, 2, 2).contains(frame));
        doc().commands().undo();
        doc().commands().undo();
        QCOMPARE(page().size(), QSizeF(1920, 1080));
    }

    // -------------------------------------------------------------------- select and move

    void selectAndMoveEveryObjectKind_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("touch");
        for (const char* kind : {"image", "graph", "shape", "empty-shape", "text", "vector", "measurement", "imported"}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 mouse").arg(QString::fromLatin1(kind)))) << QString::fromLatin1(kind) << false;
            QTest::newRow(qPrintable(QStringLiteral("%1 touch").arg(QString::fromLatin1(kind)))) << QString::fromLatin1(kind) << true;
        }
    }
    void selectAndMoveEveryObjectKind()
    {
        QFETCH(QString, kind);
        QFETCH(bool, touch);
        resetPage();
        ObjectId id;
        QPointF grab; // page point on the object where the teacher touches it
        QImage img(80, 60, QImage::Format_ARGB32);
        img.fill(Qt::darkCyan);
        // Objects are placed in the top-left area of the board (see the offscreen note below).
        const QPointF c(380, 330);
        if (kind == QLatin1String("image")) {
            id = add(ImageObject::create(doc().images().addImage(img), QSizeF(320, 240), c));
            grab = c;
        } else if (kind == QLatin1String("graph")) {
            id = add(GraphObject::create(c, QSizeF(500, 400)));
            grab = c;
        } else if (kind == QLatin1String("shape")) {
            ShapeStyle style;
            style.fill = QColor(80, 160, 255, 120);
            id = add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(c - QPointF(200, 100), QSizeF(400, 200)), style));
            grab = c;
        } else if (kind == QLatin1String("empty-shape")) {
            id = add(ShapeObject::createBox(ShapeKind::Circle, QRectF(c - QPointF(200, 200), QSizeF(400, 400)), ShapeStyle()));
            grab = c; // inside the unfilled circle
        } else if (kind == QLatin1String("text")) {
            id = add(ObjectFactory::createText(QStringLiteral("Move me"), c));
            grab = page().object(id)->sceneBounds().center();
        } else if (kind == QLatin1String("vector")) {
            id = add(GeometryObject::create(ConstructKind::Vector, {c + QPointF(-200, 100), c + QPointF(200, -100)}, Qt::cyan, true));
            grab = c;
        } else if (kind == QLatin1String("measurement")) {
            id = add(MeasurementObject::create(MeasureKind::Distance, {c - QPointF(200, 0), c + QPointF(200, 0)}, Qt::yellow));
            grab = c;
        } else {
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            img.save(&buffer, "PNG");
            QCOMPARE(insertImportedPages(doc(), 0, {ImportedPage{png, img.size(), QSizeF(80, 60)}}, QStringLiteral("scan"),
                                         ImportSizing::BoardSized, QStringLiteral("Import")), 1);
            id = page().objectAt(0)->id();
            canvas().fitPage();
            grab = page().frameRect().topLeft() + QPointF(300, 200);
        }
        m_window->activateTool(ToolId::Select);
        const QRectF before = page().object(id)->sceneBounds();
        const int count = page().objectCount();

        // Tap selects (and shows the selection frame / action bar).
        const QPoint at = toView(grab);
        // Qt's offscreen test platform has an 800 × 600 screen: synthetic touches must stay inside it.
        if (touch)
            QVERIFY2(at.x() + 95 < 800 && at.y() + 65 < 600, qPrintable(QStringLiteral("%1,%2").arg(at.x()).arg(at.y())));
        if (touch) {
            QTest::touchEvent(&canvas(), m_touch).press(0, at, &canvas());
            QTest::touchEvent(&canvas(), m_touch).release(0, at, &canvas());
        } else {
            QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, at);
        }
        QTest::qWait(30);
        QVERIFY2(canvas().selectionModel().ids() == QVector<ObjectId>{id},
                 qPrintable(QStringLiteral("selected %1 objects, page %2 of %3, objects %4")
                                .arg(canvas().selectionModel().ids().size())
                                .arg(doc().currentPageIndex())
                                .arg(doc().pageCount())
                                .arg(page().objectCount())));
        QVERIFY(m_window->selectionBar().isVisible());

        // Drag moves it by the dragged distance (in page units); nothing is drawn.
        const QPoint delta(90, 60);
        if (touch)
            touchDrag(at, at + delta);
        else
            mouseDrag(at, at + delta);
        QTest::qWait(30);
        QCOMPARE(page().objectCount(), count);
        const QRectF after = page().object(id)->sceneBounds();
        const QPointF moved = after.center() - before.center();
        const QPointF expected = QPointF(delta) / canvas().zoom();
        QVERIFY2(geom::distance(moved, expected) < 3.0 / canvas().zoom() + 1.0,
                 qPrintable(QStringLiteral("moved %1,%2 expected %3,%4").arg(moved.x()).arg(moved.y()).arg(expected.x()).arg(expected.y())));
        QCOMPARE(after.size(), before.size());
        // One undo step puts it back.
        doc().commands().undo();
        QVERIFY(geom::distance(page().object(id)->sceneBounds().center(), before.center()) < 1e-6);
        if (kind == QLatin1String("vector"))
            shot(QStringLiteral("board-select-vector"));
    }

    void penStillDrawsOverObjects()
    {
        // Drawing behaviour is unchanged: with the pen, dragging across an object draws ink.
        resetPage();
        const ObjectId id = add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(500, 400, 400, 200), ShapeStyle()));
        const QRectF before = page().object(id)->sceneBounds();
        m_window->activateTool(ToolId::Pen);
        mouseDrag(toView(QPointF(600, 500)), toView(QPointF(800, 520)));
        QCOMPARE(page().objectCount(), 2);
        QCOMPARE(page().object(id)->sceneBounds(), before);
    }

    // ---------------------------------------------------------------------------- zoom

    void zoomPresetsFitAndReset()
    {
        resetPage();
        for (qreal z : CanvasWidget::zoomPresets()) {
            canvas().setZoom(z);
            QCOMPARE(canvas().zoom(), z);
            QCOMPARE(m_window->ribbon().zoomButton()->text(), QStringLiteral("%1%").arg(qRound(z * 100)));
        }
        QCOMPARE(CanvasWidget::zoomPresets().size(), 10);
        QCOMPARE(CanvasWidget::zoomPresets().first(), 0.25);
        QCOMPARE(CanvasWidget::zoomPresets().last(), 2.5);
        canvas().setZoom(1.0);
        QTest::mouseClick(m_window->ribbon().zoomInButton(), Qt::LeftButton);
        QCOMPARE(canvas().zoom(), 1.25);
        QTest::mouseClick(m_window->ribbon().zoomOutButton(), Qt::LeftButton);
        QTest::mouseClick(m_window->ribbon().zoomOutButton(), Qt::LeftButton);
        QCOMPARE(canvas().zoom(), 0.75);
        canvas().fitWidth();
        const QRect frame = canvas().pageRectToWidget(page().frameRect());
        QVERIFY(std::abs(frame.width() - (canvas().width() - 2 * m_ui->theme.dp(20))) <= 2);
        QTest::mouseClick(m_window->ribbon().fitButton(), Qt::LeftButton);
        QVERIFY(canvas().rect().adjusted(-2, -2, 2, 2).contains(canvas().pageRectToWidget(page().frameRect())));
        // View popover presets.
        QTest::mouseClick(m_window->ribbon().zoomButton(), Qt::LeftButton);
        Popover* p = openedPopover();
        QCOMPARE(p->key(), QStringLiteral("view"));
        QTest::mouseClick(buttonIn(p, QStringLiteral("150 %")), Qt::LeftButton);
        QCOMPARE(canvas().zoom(), 1.5);
        shot(QStringLiteral("board-view-popover"));
        m_window->popovers().close();
        canvas().fitPage();
    }

    void zoomChangesNeitherGeometryNorMeasurementsNorExport()
    {
        resetPage();
        const ObjectId m = add(MeasurementObject::create(MeasureKind::Distance, {QPointF(200, 500), QPointF(480, 500)}, Qt::yellow));
        add(GeometryObject::create(ConstructKind::Vector, {QPointF(600, 700), QPointF(760, 580)}, Qt::cyan, true));
        add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(1000, 200, 400, 400), ShapeStyle()));
        const QByteArray json = QJsonDocument(page().toJson()).toJson();
        auto* measurement = static_cast<MeasurementObject*>(page().object(m));
        const QString value = measurement->valueText(doc().coordinatesFor(&page()));
        QCOMPARE(value, QStringLiteral("7 cm"));
        QImage reference;
        for (qreal z : {0.25, 0.5, 1.0, 2.5}) {
            canvas().setZoom(z);
            QTest::qWait(20);
            QCOMPARE(QJsonDocument(page().toJson()).toJson(), json); // coordinates untouched
            QCOMPARE(measurement->valueText(doc().coordinatesFor(&page())), value);
            // Export of the complete logical page.
            const auto snapshot = doc().snapshot({doc().currentPageIndex()});
            const QImage img = ImageExporter::renderPage(*snapshot->pages.front(), snapshot->images, snapshot->coordinates, 960);
            QCOMPARE(img.size(), QSize(960, 540));
            if (reference.isNull())
                reference = img;
            else
                QVERIFY2(img == reference, qPrintable(QStringLiteral("export differs at zoom %1").arg(z)));
        }
        canvas().fitPage();
    }

    // ------------------------------------------------------------------------ precision

    void propertyPanelSetsExactValues()
    {
        resetPage();
        m_window->activateTool(ToolId::Select);
        // Line length: 7 cm, then 8 cm; direction 90°.
        const ObjectId line = add(ShapeObject::createLine(ShapeKind::Line, QPointF(300, 700), QPointF(590, 610), ShapeStyle()));
        canvas().selectionModel().setSingle(line);
        QTest::qWait(30);
        auto* precisionButton = m_window->selectionBar().findChild<TouchButton*>(QStringLiteral("precision"));
        QVERIFY(precisionButton && precisionButton->isVisible());
        QTest::mouseClick(precisionButton, Qt::LeftButton);
        QVERIFY(openedPopover());
        QCOMPARE(popoverKey(), QStringLiteral("properties"));
        shot(QStringLiteral("board-properties-line"));
        enter(QStringLiteral("Length"), QStringLiteral("7"));
        const CoordinateSystem cs = doc().coordinatesFor(&page());
        QVERIFY(std::abs(precision::length(precision::definingPoints(*page().object(line)), cs) - 7.0) < 1e-9);
        QCOMPARE(field(QStringLiteral("Length"))->value(), 7.0); // panel refreshed and in sync
        enter(QStringLiteral("Length"), QStringLiteral("8,0"));
        QVERIFY(std::abs(precision::length(precision::definingPoints(*page().object(line)), cs) - 8.0) < 1e-9);
        enter(QStringLiteral("Direction"), QStringLiteral("90"));
        QVERIFY(std::abs(precision::direction(precision::definingPoints(*page().object(line)), cs) - 90.0) < 1e-9);
        // Invalid input is rejected without changing anything.
        const QVector<QPointF> kept = precision::definingPoints(*page().object(line));
        enter(QStringLiteral("Length"), QStringLiteral("abc"));
        QCOMPARE(precision::definingPoints(*page().object(line)), kept);
        m_window->popovers().close();
        QTest::qWait(150);

        // Angle 90°.
        const ObjectId angle = add(MeasurementObject::create(MeasureKind::Angle, {QPointF(900, 500), QPointF(700, 500), QPointF(820, 380)}, Qt::yellow));
        m_window->editObject(angle);
        QVERIFY(openedPopover());
        enter(QStringLiteral("Angle"), QStringLiteral("90"));
        QVERIFY(std::abs(precision::angle(precision::definingPoints(*page().object(angle)), cs) - 90.0) < 1e-9);
        shot(QStringLiteral("board-properties-angle"));
        m_window->popovers().close();
        QTest::qWait(150);

        // Slope: rise 4, run 2 -> slope 2, angle 63.435°.
        const ObjectId slope = add(MeasurementObject::create(MeasureKind::Slope, {QPointF(1100, 700), QPointF(1200, 650)}, Qt::yellow));
        m_window->editObject(slope);
        QVERIFY(openedPopover());
        enter(QStringLiteral("Run"), QStringLiteral("2"));
        enter(QStringLiteral("Rise"), QStringLiteral("4"));
        precision::SlopeValues v = precision::slopeValues(precision::definingPoints(*page().object(slope)), cs);
        QVERIFY(std::abs(v.rise - 4) < 1e-9 && std::abs(v.run - 2) < 1e-9);
        QCOMPARE(field(QStringLiteral("Slope"))->value(), 2.0);
        QVERIFY(std::abs(field(QStringLiteral("Angle"))->value() - 63.435) < 1e-3);
        shot(QStringLiteral("board-properties-slope"));
        enter(QStringLiteral("Angle"), QStringLiteral("45"));
        v = precision::slopeValues(precision::definingPoints(*page().object(slope)), cs);
        QVERIFY(std::abs(v.slope - 1) < 1e-9);
        m_window->popovers().close();
        QTest::qWait(150);
    }

    void scaleAndVectorThroughPanels()
    {
        resetPage();
        // Scale 10 cm = 1 km through the scale panel presets.
        m_window->popovers().open(QStringLiteral("scale"), m_window->ribbon().moreButton());
        Popover* p = openedPopover();
        QVERIFY(p);
        QTest::mouseClick(buttonIn(p, QStringLiteral("10 cm = 1 km")), Qt::LeftButton);
        QVERIFY(doc().coordinates().usesScale());
        QCOMPARE(doc().coordinates().scale().text(), QStringLiteral("10 cm = 1 km"));
        shot(QStringLiteral("board-scale"));
        m_window->popovers().close();
        QTest::qWait(150);

        // Vector: magnitude 2 km, direction 90° -> a 20 cm arrow pointing up.
        const ObjectId vector = add(GeometryObject::create(ConstructKind::Vector, {QPointF(900, 900), QPointF(1000, 860)}, Qt::cyan, true));
        m_window->editObject(vector);
        QVERIFY(openedPopover());
        enter(QStringLiteral("Magnitude"), QStringLiteral("2"));
        enter(QStringLiteral("Direction"), QStringLiteral("90"));
        const CoordinateSystem cs = doc().coordinatesFor(&page());
        const QVector<QPointF> pts = precision::definingPoints(*page().object(vector));
        QVERIFY(std::abs(cs.toReal(precision::length(pts, cs)) - 2.0) < 1e-9);
        QVERIFY(std::abs(precision::length(pts, cs) - 20.0) < 1e-9);
        QVERIFY(std::abs(precision::direction(pts, cs) - 90.0) < 1e-9);
        shot(QStringLiteral("board-properties-vector"));
        m_window->popovers().close();
        QTest::qWait(150);

        // Line: real length 1 km <-> board 10 cm, both directions.
        const ObjectId line = add(MeasurementObject::create(MeasureKind::Distance, {QPointF(200, 300), QPointF(480, 300)}, Qt::yellow));
        m_window->editObject(line);
        QVERIFY(openedPopover());
        QCOMPARE(field(QStringLiteral("Length (real)"))->value(), 0.7); // 7 cm drawn
        enter(QStringLiteral("Length (real)"), QStringLiteral("1"));
        QVERIFY(std::abs(precision::length(precision::definingPoints(*page().object(line)), cs) - 10.0) < 1e-9);
        QCOMPARE(field(QStringLiteral("Length (board)"))->value(), 10.0);
        m_window->popovers().close();
        QTest::qWait(150);

        // Square 10 × 10 cm = 1 km², perimeter 4 km.
        const ObjectId square = add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(1200, 300, 400, 400), ShapeStyle()));
        m_window->editObject(square);
        QVERIFY(openedPopover());
        QCOMPARE(field(QStringLiteral("Width (real)"))->value(), 1.0);
        QCOMPARE(field(QStringLiteral("Area"))->value(), 1.0);
        QCOMPARE(field(QStringLiteral("Perimeter"))->value(), 4.0);
        shot(QStringLiteral("board-properties-square"));
        m_window->popovers().close();
        QTest::qWait(150);
        doc().commands().push(std::make_unique<SetScaleCommand>(MeasureScale(), QString()));
    }

    // -------------------------------------------------------------------- Magic Equation

    void magicEquationAcceptReplacesHandwritingInOneStep()
    {
        resetPage();
        m_window->activateTool(ToolId::Select);
        testink::Writer w(7);
        addStrokes(testink::write(w, QStringLiteral("x^2 + 1"), QPointF(600, 500)));
        const int strokes = page().objectCount();
        QVERIFY(strokes >= 5);
        QTest::qWait(30);
        auto* magicButton = m_window->selectionBar().findChild<TouchButton*>(QStringLiteral("magic"));
        QVERIFY(magicButton && magicButton->isVisible());
        QTest::mouseClick(magicButton, Qt::LeftButton);
        Popover* p = openedPopover();
        QVERIFY(p);
        auto* magic = p->findChild<MagicEquationPanel*>();
        QVERIFY(magic);
        QTRY_COMPARE(magic->state(), MagicEquationPanel::State::Result);
        QCOMPARE(magic->latex(), QStringLiteral("x^{2}+1"));
        // Preview only: the handwriting is still there, no equation yet.
        QCOMPARE(page().objectCount(), strokes);
        shot(QStringLiteral("board-magic-preview"));
        magic->accept();
        QTest::qWait(50);
        QCOMPARE(page().objectCount(), 1);
        QCOMPARE(page().objectAt(0)->type(), ObjectType::Equation);
        QCOMPARE(static_cast<EquationObject*>(page().objectAt(0))->latex(), QStringLiteral("x^{2}+1"));
        shot(QStringLiteral("board-magic-accepted"));
        // One undo restores the original handwriting exactly.
        doc().commands().undo();
        QCOMPARE(page().objectCount(), strokes);
        for (const auto& o : page().objects())
            QCOMPARE(o->type(), ObjectType::Stroke);
    }

    void magicEquationCancelAndEditLeaveHandwriting()
    {
        resetPage();
        testink::Writer w(9);
        addStrokes(testink::write(w, QStringLiteral("2x + 3"), QPointF(600, 500)));
        const QByteArray before = QJsonDocument(page().toJson()).toJson();
        m_window->openMagicEquation();
        auto* magic = openedPopover()->findChild<MagicEquationPanel*>();
        QTRY_COMPARE(magic->state(), MagicEquationPanel::State::Result);
        // Edit the recognised LaTeX before accepting.
        magic->edit();
        QLineEdit* edit = openedPopover()->findChild<QLineEdit*>();
        QVERIFY(edit && edit->isVisible());
        QCOMPARE(QJsonDocument(page().toJson()).toJson(), before);
        // Cancel: nothing changes.
        magic->cancel();
        QTest::qWait(200);
        QCOMPARE(QJsonDocument(page().toJson()).toJson(), before);
        QVERIFY(!m_window->popoverHost().isOpen());
        // Accept after editing uses the edited formula.
        m_window->openMagicEquation();
        magic = openedPopover()->findChild<MagicEquationPanel*>();
        QTRY_COMPARE(magic->state(), MagicEquationPanel::State::Result);
        magic->edit();
        edit = openedPopover()->findChild<QLineEdit*>();
        edit->setText(QStringLiteral("2x+3=7"));
        magic->accept();
        QTest::qWait(50);
        QCOMPARE(page().objectCount(), 1);
        QCOMPARE(static_cast<EquationObject*>(page().objectAt(0))->latex(), QStringLiteral("2x+3=7"));
    }

    void magicEquationFailureOffersTryAgainEditOrCancel()
    {
        resetPage();
        QVector<QPointF> scribble;
        for (int i = 0; i < 40; ++i)
            scribble << QPointF(600 + i * 8, (i % 2) ? 400 : 480);
        addStrokes({scribble});
        const QByteArray before = QJsonDocument(page().toJson()).toJson();
        m_window->openMagicEquation();
        auto* magic = openedPopover()->findChild<MagicEquationPanel*>();
        QTRY_COMPARE(magic->state(), MagicEquationPanel::State::Failed);
        QVERIFY(buttonIn(openedPopover(), QStringLiteral("Try again")));
        QVERIFY(buttonIn(openedPopover(), QStringLiteral("Edit manually")));
        QVERIFY(buttonIn(openedPopover(), QStringLiteral("Cancel")));
        shot(QStringLiteral("board-magic-failed"));
        QCOMPARE(QJsonDocument(page().toJson()).toJson(), before); // never silently replaced
        // Try again gives a low-confidence best guess that still needs the teacher's decision.
        magic->tryAgain();
        QTRY_VERIFY(magic->state() != MagicEquationPanel::State::Recognizing);
        QCOMPARE(QJsonDocument(page().toJson()).toJson(), before);
        if (magic->state() == MagicEquationPanel::State::Result)
            QVERIFY(magic->confidence() < 0.6);
        // Edit manually opens the Equation Making Centre; the handwriting stays.
        magic->editManually();
        QTest::qWait(250);
        QCOMPARE(popoverKey(), QStringLiteral("equation"));
        QCOMPARE(QJsonDocument(page().toJson()).toJson(), before);
        shot(QStringLiteral("board-magic-manual"));
        m_window->popovers().close();
        QTest::qWait(150);
    }

    void magicNeedsExplicitHandwritingSelection()
    {
        resetPage();
        const ObjectId shape = add(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(500, 400, 200, 100), ShapeStyle()));
        m_window->activateTool(ToolId::Select);
        canvas().selectionModel().setSingle(shape);
        QTest::qWait(30);
        auto* magicButton = m_window->selectionBar().findChild<TouchButton*>(QStringLiteral("magic"));
        QVERIFY(!magicButton->isVisible()); // only offered for handwriting
        m_window->openMagicEquation();
        QTest::qWait(150);
        QVERIFY(m_window->popoverHost().currentKey() != QLatin1String("magic"));
        QCOMPARE(page().objectCount(), 1);
    }
};

QTEST_MAIN(TestBoardUi)
#include "tst_board_ui.moc"
