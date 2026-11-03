// Magic Highlighter: temporary strokes that fade away and never become part of the lesson.
// Unit tests of the effect layer plus end-to-end tests on the real window (Pen popover, mouse,
// touch and stylus input, undo/redo, saving). Set CLASSBOARD_SCREENSHOTS=<dir> for screenshots.
#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "canvas/MagicHighlightLayer.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/PageOperations.h"
#include "document/StrokeObject.h"
#include "storage/ProjectSerializer.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/UiContext.h"
#include "ui/widgets/SegmentedControl.h"
#include "ui/widgets/TouchButton.h"

#include <QPainter>
#include <QSignalSpy>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QtTest>

using namespace cb;

namespace {
QVector<StrokePoint> line(QPointF a, QPointF b, int n = 10)
{
    QVector<StrokePoint> pts;
    for (int i = 0; i <= n; ++i)
        pts.push_back({a + (b - a) * (double(i) / n), 1.0f});
    return pts;
}

InkStyle magicInk()
{
    InkStyle ink;
    ink.color = QColor(255, 235, 59);
    ink.width = 22;
    ink.style = StrokeStyle::Highlighter;
    return ink;
}
} // namespace

class TestMagicLayer : public QObject
{
    Q_OBJECT
private slots:
    void opacityCurve()
    {
        QCOMPARE(MagicHighlightLayer::opacityAt(0), 1.0);
        QCOMPARE(MagicHighlightLayer::opacityAt(MAGIC_HIGHLIGHTER_DURATION_MS), 1.0);
        const qreal half = MagicHighlightLayer::opacityAt(MAGIC_HIGHLIGHTER_DURATION_MS + MAGIC_HIGHLIGHTER_FADE_MS / 2);
        QVERIFY(std::abs(half - 0.5) < 1e-9);
        QCOMPARE(MagicHighlightLayer::opacityAt(MAGIC_HIGHLIGHTER_DURATION_MS + MAGIC_HIGHLIGHTER_FADE_MS), 0.0);
        QCOMPARE(MagicHighlightLayer::opacityAt(60000), 0.0);
        // Smooth and monotonic: no jumps while fading.
        qreal previous = 1.0;
        for (int ms = MAGIC_HIGHLIGHTER_DURATION_MS; ms <= MAGIC_HIGHLIGHTER_DURATION_MS + MAGIC_HIGHLIGHTER_FADE_MS; ms += 10) {
            const qreal o = MagicHighlightLayer::opacityAt(ms);
            QVERIFY(o <= previous + 1e-12);
            QVERIFY(previous - o < 0.02);
            previous = o;
        }
        QVERIFY(MAGIC_HIGHLIGHTER_DURATION_MS == 5000);
    }

    void lifetime()
    {
        MagicHighlightLayer layer;
        layer.setDurations(1000, 300);
        QSignalSpy repaints(&layer, &MagicHighlightLayer::repaintRequested);
        const PageId page = QUuid::createUuid();
        layer.add(page, line(QPointF(0, 0), QPointF(100, 0)), magicInk());
        QCOMPARE(layer.count(), 1);
        QCOMPARE(layer.countOnPage(page), 1);
        QCOMPARE(layer.countOnPage(QUuid::createUuid()), 0);
        QCOMPARE(repaints.count(), 1); // shown at once
        QTest::qWait(150);
        QCOMPARE(layer.count(), 1); // still fully visible
        QTRY_COMPARE_WITH_TIMEOUT(layer.count(), 0, 5000);
        QVERIFY2(repaints.count() >= 4, "the fade must be animated, not a jump"); // frames while fading
        // The timer stops when nothing is left: no further repaints.
        const int after = repaints.count();
        QTest::qWait(150);
        QCOMPARE(repaints.count(), after);
    }

    void clearDropsEverything()
    {
        MagicHighlightLayer layer;
        const PageId page = QUuid::createUuid();
        layer.add(page, line(QPointF(0, 0), QPointF(100, 0)), magicInk());
        layer.add(page, line(QPointF(0, 50), QPointF(100, 50)), magicInk());
        QCOMPARE(layer.count(), 2);
        layer.clear();
        QCOMPARE(layer.count(), 0);
    }

    void paintsAndFades()
    {
        const PageId page = QUuid::createUuid();
        MagicHighlightLayer layer;
        layer.setDurations(1000, 100);
        layer.add(page, line(QPointF(10, 50), QPointF(190, 50)), magicInk());
        auto render = [&]() {
            QImage img(200, 100, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::black);
            QPainter p(&img);
            layer.paint(p, page);
            return img;
        };
        const QImage visible = render();
        QVERIFY(QColor(visible.pixel(100, 50)).red() > 60);
        QCOMPARE(QColor(visible.pixel(100, 5)), QColor(Qt::black));
        QTRY_COMPARE_WITH_TIMEOUT(layer.count(), 0, 5000);
        QCOMPARE(QColor(render().pixel(100, 50)), QColor(Qt::black));
    }
};

class TestMagicHighlighterUi : public QObject
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
    ToolSettings& tools() { return m_window->toolSettings(); }
    MagicHighlightLayer& magic() { return canvas().magicHighlights(); }

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

    void tablet(QEvent::Type type, const QPoint& pos, qreal pressure)
    {
        const QPointF global = canvas().mapToGlobal(pos);
        QTabletEvent e(type, QPointF(pos), global, QTabletEvent::Stylus, QTabletEvent::Pen, pressure, 0, 0, 0, 0, 0,
                       Qt::NoModifier, 1, type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton,
                       type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton);
        QApplication::sendEvent(&canvas(), &e);
    }

    void stylusDrag(const QPoint& from, const QPoint& to, int steps = 12)
    {
        tablet(QEvent::TabletPress, from, 0.5);
        for (int i = 1; i <= steps; ++i)
            tablet(QEvent::TabletMove, from + (to - from) * i / steps, 0.7);
        tablet(QEvent::TabletRelease, to, 0.0);
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
        QTest::qWait(100);
        magic().clear();
        pageops::clearPage(doc(), 0);
        doc().commands().clear();
        doc().markSaved();
        tools().setPenStyle(StrokeStyle::Pen);
        m_window->activateTool(ToolId::Pen);
    }

    QByteArray savedBytes() { return ProjectSerializer::serialize(doc().copyContents()); }

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
        // Short lifetime so the tests do not wait five seconds per stroke.
        magic().setDurations(1500, 300);
    }

    void cleanupTestCase()
    {
        doc().markSaved();
        m_window.reset();
    }

    void penPopoverOffersMagicHighlighter()
    {
        resetPage();
        QVERIFY(m_ui->icons.exists(QStringLiteral("magic-highlighter")));
        QTest::mouseClick(m_window->ribbon().penButton(), Qt::LeftButton);
        QTest::qWait(250);
        if (m_window->popoverHost().currentKey() != QLatin1String("pen")) {
            QTest::mouseClick(m_window->ribbon().penButton(), Qt::LeftButton);
            QTest::qWait(250);
        }
        Popover* popover = m_window->popoverHost().current();
        QVERIFY(popover);
        auto* style = popover->findChild<SegmentedControl*>(QStringLiteral("penStyle"));
        QVERIFY(style);
        // Pen | Highlighter | Magic Highlighter | Dashed | Dotted
        QTest::mouseClick(style, Qt::LeftButton, Qt::NoModifier, style->segmentRect(2).center().toPoint());
        QVERIFY(tools().magicHighlighter());
        QCOMPARE(tools().penStyle(), StrokeStyle::Pen); // the style underneath is kept
        QCOMPARE(style->currentIndex(), 2);
        shot(QStringLiteral("magic-highlighter-popover"));
        // Choosing the ordinary highlighter leaves magic mode: they are independent.
        QTest::mouseClick(style, Qt::LeftButton, Qt::NoModifier, style->segmentRect(1).center().toPoint());
        QVERIFY(!tools().magicHighlighter());
        QCOMPARE(tools().penStyle(), StrokeStyle::Highlighter);
        m_window->popovers().close();
    }

    void magicStrokeIsTemporaryAndNeverSaved()
    {
        resetPage();
        const QByteArray before = savedBytes();
        const int undoIndex = doc().commands().index();
        tools().setMagicHighlighter(true);
        mouseDrag(QPoint(300, 300), QPoint(700, 320));
        // Shown at once, as a temporary highlight only.
        QCOMPARE(magic().count(), 1);
        QCOMPARE(page().objectCount(), 0);
        QCOMPARE(doc().commands().index(), undoIndex);
        QVERIFY(!doc().commands().canUndo());
        QVERIFY(!doc().isModified());
        QCOMPARE(savedBytes(), before);
        // Visible on the board ...
        QTest::qWait(50);
        const QImage visible = canvas().grab().toImage();
        const QColor onStroke = visible.pixelColor(QPoint(500, 310) * visible.devicePixelRatio());
        shot(QStringLiteral("magic-highlighter-visible"));
        // ... then fades away by itself.
        QTRY_COMPARE_WITH_TIMEOUT(magic().count(), 0, 6000);
        QTest::qWait(50);
        const QImage gone = canvas().grab().toImage();
        const QColor afterwards = gone.pixelColor(QPoint(500, 310) * gone.devicePixelRatio());
        QVERIFY2(onStroke != afterwards, "the highlight was not drawn");
        const QColor background = gone.pixelColor(QPoint(500, 200) * gone.devicePixelRatio());
        QCOMPARE(afterwards, background);
        QCOMPARE(page().objectCount(), 0);
        QVERIFY(!doc().isModified());
        QCOMPARE(savedBytes(), before);
        tools().setMagicHighlighter(false);
    }

    void undoRedoIgnoreMagicStrokes()
    {
        resetPage();
        mouseDrag(QPoint(300, 250), QPoint(700, 250)); // normal ink
        QCOMPARE(page().objectCount(), 1);
        const int index = doc().commands().index();
        tools().setMagicHighlighter(true);
        mouseDrag(QPoint(300, 350), QPoint(700, 350));
        mouseDrag(QPoint(300, 400), QPoint(700, 400));
        QCOMPARE(magic().count(), 2);
        QCOMPARE(doc().commands().index(), index);
        // Undo removes the real stroke, not a highlight; redo restores it.
        doc().commands().undo();
        QCOMPARE(page().objectCount(), 0);
        QCOMPARE(magic().count(), 2);
        doc().commands().redo();
        QCOMPARE(page().objectCount(), 1);
        QCOMPARE(doc().commands().index(), index);
        tools().setMagicHighlighter(false);
        magic().clear();
    }

    void worksWithTouchStylusAndSeveralFingers()
    {
        resetPage();
        magic().setDurations(20000, 300); // keep them while waiting for pen proximity to end
        tools().setMagicHighlighter(true);
        // Finger.
        QTest::touchEvent(&canvas(), m_touch).press(0, QPoint(200, 200), &canvas());
        for (int i = 1; i <= 10; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, QPoint(200 + i * 30, 200 + i * 5), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, QPoint(500, 250), &canvas());
        QCOMPARE(magic().count(), 1);
        // Stylus.
        QTest::qWait(int(InputManager::kStylusProximityMs) + 50);
        stylusDrag(QPoint(200, 300), QPoint(600, 320));
        QCOMPARE(magic().count(), 2);
        QTest::qWait(int(InputManager::kStylusProximityMs) + 50);
        // Several students at once (multi-user touch): one highlight per finger.
        canvas().input().setMultiUserTouch(true);
        QTest::touchEvent(&canvas(), m_touch).press(1, QPoint(150, 420), &canvas()).press(2, QPoint(550, 420), &canvas());
        for (int i = 1; i <= 8; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(1, QPoint(150 + i * 12, 420 + i * 6), &canvas()).move(2, QPoint(550 + i * 12, 420 + i * 6), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(1, QPoint(246, 468), &canvas()).release(2, QPoint(646, 468), &canvas());
        canvas().input().setMultiUserTouch(false);
        QCOMPARE(magic().count(), 4);
        QCOMPARE(page().objectCount(), 0);
        QVERIFY(!doc().commands().canUndo());
        tools().setMagicHighlighter(false);
        magic().setDurations(1500, 300);
        QTRY_COMPARE_WITH_TIMEOUT(magic().count(), 0, 6000);
    }

    void normalHighlighterStillCreatesObjects()
    {
        resetPage();
        tools().setMagicHighlighter(true);
        tools().setPenStyle(StrokeStyle::Highlighter); // leaves magic mode
        QVERIFY(!tools().magicHighlighter());
        mouseDrag(QPoint(300, 300), QPoint(700, 300));
        QCOMPARE(page().objectCount(), 1);
        QCOMPARE(static_cast<StrokeObject*>(page().objectAt(0))->ink().style, StrokeStyle::Highlighter);
        QCOMPARE(magic().count(), 0);
        QVERIFY(doc().commands().canUndo());
        tools().setPenStyle(StrokeStyle::Pen);
    }

    void pageChangeAndNewLessonDropHighlights()
    {
        resetPage();
        tools().setMagicHighlighter(true);
        mouseDrag(QPoint(300, 300), QPoint(700, 300));
        QCOMPARE(magic().count(), 1);
        pageops::newPage(doc(), 0); // shows the new page
        QCOMPARE(doc().currentPageIndex(), 1);
        QCOMPARE(magic().count(), 0);
        pageops::deletePage(doc(), 1);
        doc().setCurrentPageIndex(0);
        tools().setMagicHighlighter(false);
    }

    void wipeDoesNotTouchHighlights()
    {
        resetPage();
        tools().setMagicHighlighter(true);
        mouseDrag(QPoint(300, 350), QPoint(700, 350));
        QCOMPARE(magic().count(), 1);
        // Three fingers held together: the wipe erases document content only.
        QTest::touchEvent(&canvas(), m_touch).press(0, QPoint(480, 320), &canvas()).press(1, QPoint(510, 315), &canvas()).press(2, QPoint(540, 322), &canvas());
        for (int i = 1; i <= 6; ++i)
            QTest::touchEvent(&canvas(), m_touch).move(0, QPoint(480, 320 + i * 10), &canvas()).move(1, QPoint(510, 315 + i * 10), &canvas()).move(2, QPoint(540, 322 + i * 10), &canvas());
        QTest::touchEvent(&canvas(), m_touch).release(0, QPoint(480, 380), &canvas()).release(1, QPoint(510, 375), &canvas()).release(2, QPoint(540, 382), &canvas());
        QCOMPARE(magic().count(), 1); // the fingers drew nothing and did not add highlights
        QCOMPARE(page().objectCount(), 0);
        QVERIFY(!doc().commands().canUndo());
        tools().setMagicHighlighter(false);
        magic().clear();
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    {
        TestMagicLayer t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestMagicHighlighterUi t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}
#include "tst_highlighter.moc"
