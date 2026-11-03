// Integration tests driving the real main window: drawing, tools, popovers, pages, undo/redo,
// editing and saving. Set CLASSBOARD_SCREENSHOTS=<dir> to store screenshots of every popover.
#include "app/AppSettings.h"
#include "app/LessonController.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "geometry/InstrumentLayer.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest>

using namespace cb;

class TestUi : public QObject
{
    Q_OBJECT
private:
    std::unique_ptr<UiContext> m_ui;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<MainWindow> m_window;
    QString m_shots;

    CanvasWidget& canvas() { return m_window->canvas(); }
    Document& doc() { return m_window->document(); }
    Page& page() { return *doc().currentPage(); }

    void drag(const QPoint& from, const QPoint& to, int steps = 12)
    {
        QTest::mousePress(&canvas(), Qt::LeftButton, Qt::NoModifier, from);
        for (int i = 1; i <= steps; ++i) {
            const QPoint p = from + (to - from) * i / steps;
            QMouseEvent move(QEvent::MouseMove, p, canvas().mapToGlobal(p), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&canvas(), &move);
        }
        QTest::mouseRelease(&canvas(), Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(10);
    }

    void shot(const QString& name)
    {
        if (m_shots.isEmpty())
            return;
        QTest::qWait(250);
        m_window->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    QPoint pageToCanvas(const QPointF& p) { return canvas().view().pageToView(p).toPoint(); }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(classboard);
        QStandardPaths::setTestModeEnabled(true);
        m_shots = qEnvironmentVariable("CLASSBOARD_SCREENSHOTS");
        QCoreApplication::setApplicationVersion(QStringLiteral(CLASSBOARD_VERSION));
        m_ui = std::make_unique<UiContext>();
        QVERIFY(m_ui->theme.load(QStringLiteral(":/themes/dark.json")));
        m_settings = std::make_unique<AppSettings>();
        m_settings->setUiScale(1.0);
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

    void drawAndUndo()
    {
        m_window->activateTool(ToolId::Pen);
        drag(QPoint(300, 300), QPoint(700, 420));
        QCOMPARE(page().objectCount(), 1);
        QCOMPARE(page().objectAt(0)->type(), ObjectType::Stroke);
        drag(QPoint(300, 500), QPoint(700, 520));
        QCOMPARE(page().objectCount(), 2);
        doc().commands().undo();
        QCOMPARE(page().objectCount(), 1);
        doc().commands().redo();
        QCOMPARE(page().objectCount(), 2);
        shot(QStringLiteral("01-drawing"));
    }

    void strokeEraser()
    {
        m_window->toolSettings().setEraserMode(EraserMode::Stroke);
        m_window->activateTool(ToolId::Eraser);
        drag(QPoint(500, 450), QPoint(500, 560));
        QCOMPARE(page().objectCount(), 1);
        doc().commands().undo();
        QCOMPARE(page().objectCount(), 2);
    }

    void areaEraserSplitsInk()
    {
        m_window->toolSettings().setEraserMode(EraserMode::Area);
        m_window->toolSettings().setEraserSize(30);
        m_window->activateTool(ToolId::Eraser);
        const int before = page().objectCount();
        // Wipe vertically through the middle of the second (nearly horizontal) stroke.
        drag(QPoint(500, 470), QPoint(500, 560));
        QCOMPARE(page().objectCount(), before + 1);
        doc().commands().undo();
        QCOMPARE(page().objectCount(), before);
    }

    void shapesAndSelection()
    {
        m_window->toolSettings().setShapeKind(ShapeKind::Rectangle);
        m_window->activateTool(ToolId::Shape);
        drag(QPoint(900, 250), QPoint(1150, 420));
        DocumentObject* shape = page().objectAt(page().objectCount() - 1);
        QCOMPARE(shape->type(), ObjectType::Shape);
        const QPointF before = shape->position();
        const ObjectId id = shape->id();

        m_window->activateTool(ToolId::Select);
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, QPoint(900, 330));
        QCOMPARE(canvas().selectionModel().ids(), QVector<ObjectId>{id});
        shot(QStringLiteral("02-selection"));
        drag(QPoint(905, 330), QPoint(1005, 380));
        const QPointF after = page().object(id)->position();
        QVERIFY(after.x() > before.x());
        doc().commands().undo();
        QCOMPARE(page().object(id)->position(), before);
    }

    void textTool()
    {
        m_window->activateTool(ToolId::Text);
        const int before = page().objectCount();
        QTest::mouseClick(&canvas(), Qt::LeftButton, Qt::NoModifier, QPoint(300, 700));
        auto* editor = canvas().findChild<QPlainTextEdit*>();
        QVERIFY(editor);
        QVERIFY(editor->isVisible());
        QTest::keyClicks(editor, QStringLiteral("Water cycle"));
        shot(QStringLiteral("03-text-editing"));
        QTest::keyClick(editor, Qt::Key_Escape);
        QTest::qWait(20);
        QCOMPARE(page().objectCount(), before + 1);
        QCOMPARE(page().objectAt(before)->type(), ObjectType::Text);
    }

    void popovers_data()
    {
        QTest::addColumn<QString>("key");
        for (const char* k : {"more", "pen", "eraser", "select", "shapes", "geometry", "measure", "equation", "function",
                              "text", "table", "lesson", "templates", "import", "export", "pdf", "pptx", "settings",
                              "pages", "color", "insert", "edit", "pageactions", "pagesize", "background", "view",
                              "scale"})
            QTest::newRow(k) << QString::fromLatin1(k);
    }
    void popovers()
    {
        QFETCH(QString, key);
        m_window->activateTool(ToolId::Pen);
        m_window->popovers().open(key, m_window->ribbon().moreButton());
        QTest::qWait(200);
        Popover* p = m_window->popoverHost().current();
        QVERIFY(p);
        QCOMPARE(p->key(), key);
        QVERIFY(p->isVisible());
        const QRect hostRect = m_window->popoverHost().rect();
        QVERIFY2(hostRect.adjusted(-30, -30, 30, 30).contains(p->geometry()), qPrintable(key));
        shot(QStringLiteral("popover-") + key);
        m_window->popovers().close();
        QTest::qWait(150);
        QVERIFY(!m_window->popoverHost().isOpen());
    }

    void ribbonPenOpensPopoverOnSecondTap()
    {
        m_window->activateTool(ToolId::Eraser);
        QTest::mouseClick(m_window->ribbon().penButton(), Qt::LeftButton);
        QCOMPARE(canvas().tools().activeToolId(), ToolId::Pen);
        QVERIFY(!m_window->popoverHost().isOpen());
        QTest::mouseClick(m_window->ribbon().penButton(), Qt::LeftButton);
        QTest::qWait(50);
        QCOMPARE(m_window->popoverHost().currentKey(), QStringLiteral("pen"));
        shot(QStringLiteral("04-pen-popover"));
        // Tapping outside closes it.
        QTest::mouseClick(&m_window->popoverHost(), Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
        QTest::qWait(150);
        QVERIFY(!m_window->popoverHost().isOpen());
    }

    void equationInsert()
    {
        m_window->popovers().open(QStringLiteral("equation"), m_window->ribbon().moreButton());
        QTest::qWait(100);
        auto* edit = m_window->popoverHost().current()->findChild<QLineEdit*>();
        QVERIFY(edit);
        edit->setText(QStringLiteral("x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}"));
        shot(QStringLiteral("05-equation-popover"));
        const int before = page().objectCount();
        QTest::keyClick(edit, Qt::Key_Return);
        QTest::qWait(150);
        QCOMPARE(page().objectCount(), before + 1);
        QCOMPARE(page().objectAt(before)->type(), ObjectType::Equation);
    }

    void graphInsert()
    {
        canvas().selectionModel().clear();
        m_window->popovers().open(QStringLiteral("function"), m_window->ribbon().moreButton());
        QTest::qWait(100);
        TouchButton* insert = nullptr;
        for (TouchButton* b : m_window->popoverHost().current()->findChildren<TouchButton*>())
            if (b->text() == QStringLiteral("Insert graph"))
                insert = b;
        QVERIFY(insert);
        const int before = page().objectCount();
        QTest::mouseClick(insert, Qt::LeftButton);
        QTest::qWait(150);
        QCOMPARE(page().objectCount(), before + 1);
        QCOMPARE(page().objectAt(before)->type(), ObjectType::Graph);
        QCOMPARE(m_window->popoverHost().currentKey(), QStringLiteral("function"));
        shot(QStringLiteral("06-graph"));
        m_window->popovers().close();
        QTest::qWait(150);
    }

    void rulerStraightensInk()
    {
        InstrumentLayer& instruments = canvas().instruments();
        instruments.show(Instrument::Kind::Ruler, QPointF(960, 800));
        Instrument* ruler = instruments.find(Instrument::Kind::Ruler);
        QVERIFY(ruler);
        m_window->activateTool(ToolId::Pen);
        const int before = page().objectCount();
        // Draw a wobbly line starting on the ruler's top edge.
        const QPointF edgeStart = ruler->toPage(QPointF(-300, -48));
        const QPoint a = pageToCanvas(edgeStart + QPointF(0, -3));
        QTest::mousePress(&canvas(), Qt::LeftButton, Qt::NoModifier, a);
        for (int i = 1; i <= 10; ++i) {
            const QPoint p = a + QPoint(i * 30, (i % 2) * 12 - 6);
            QMouseEvent move(QEvent::MouseMove, p, canvas().mapToGlobal(p), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&canvas(), &move);
        }
        QTest::mouseRelease(&canvas(), Qt::LeftButton, Qt::NoModifier, a + QPoint(300, 0));
        QCOMPARE(page().objectCount(), before + 1);
        auto* stroke = static_cast<StrokeObject*>(page().objectAt(before));
        for (const QPointF& p : stroke->pagePoints())
            QVERIFY(qAbs(ruler->toLocal(p).y() + 48) < 0.5);
        shot(QStringLiteral("07-ruler"));
        instruments.hide(Instrument::Kind::Ruler);
    }

    void pagesNavigation()
    {
        QTest::mouseClick(m_window->ribbon().newPageButton(), Qt::LeftButton);
        QCOMPARE(doc().pageCount(), 2);
        QCOMPARE(doc().currentPageIndex(), 1);
        QCOMPARE(doc().currentPage()->objectCount(), 0);
        doc().setCurrentPageIndex(0);
        QVERIFY(doc().currentPage()->objectCount() > 0);
        // Undo of the page insertion from page 1 removes it again.
        doc().commands().undo();
        QCOMPARE(doc().pageCount(), 1);
    }

    void saveAndOpen()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("lesson.classboard"));
        const int objects = page().objectCount();
        doc().setFilePath(path);
        bool done = false;
        bool ok = false;
        m_window->lesson().save([&](bool success) {
            done = true;
            ok = success;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY(ok);
        QVERIFY(!doc().isModified());
        doc().resetToNew(TemplateSpec());
        QCOMPARE(page().objectCount(), 0);
        m_window->lesson().openFile(path);
        QTRY_COMPARE_WITH_TIMEOUT(page().objectCount(), objects, 10000);
        QCOMPARE(doc().filePath(), path);
        shot(QStringLiteral("08-reopened"));
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
