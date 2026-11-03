// Settings → Ribbon size on the real window: the bottom ribbon gets slimmer or bigger while the
// Settings menu stays open, menus keep the interface size, and the size is remembered (also when
// the interface size changes). Set CLASSBOARD_SCREENSHOTS=<dir> for screenshots.
#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "document/Document.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"
#include "ui/widgets/TouchSlider.h"

#include <QtTest>

using namespace cb;

class TestRibbonSize : public QObject
{
    Q_OBJECT
private:
    std::unique_ptr<UiContext> m_ui;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<MainWindow> m_window;
    QString m_shots;
    int m_normalHeight = 0;

    void shot(const QString& name)
    {
        if (m_shots.isEmpty())
            return;
        QTest::qWait(250);
        m_window->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    TouchSlider* openSettings()
    {
        m_window->popovers().open(QStringLiteral("settings"), m_window->ribbon().moreButton());
        QTest::qWait(150);
        Popover* p = m_window->popoverHost().current();
        return p ? p->findChild<TouchSlider*>(QStringLiteral("ribbonSize")) : nullptr;
    }

    /// Drags the slider to a fraction of its track, like a finger would.
    void drag(TouchSlider* slider, qreal fraction)
    {
        const int y = m_ui->theme.dpi(49);
        const int x = qRound(fraction * slider->width());
        QTest::mousePress(slider, Qt::LeftButton, {}, QPoint(slider->width() / 2, y));
        QMouseEvent move(QEvent::MouseMove, QPointF(x, y), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(slider, &move);
        QTest::mouseRelease(slider, Qt::LeftButton, {}, QPoint(x, y));
        QTest::qWait(100);
    }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(classboard);
        QStandardPaths::setTestModeEnabled(true);
        m_shots = qEnvironmentVariable("CLASSBOARD_SCREENSHOTS");
        m_ui = std::make_unique<UiContext>();
        QVERIFY(m_ui->theme.load(QStringLiteral(":/themes/dark.json")));
        m_settings = std::make_unique<AppSettings>();
        m_settings->setUiScale(1.0);
        m_settings->setRibbonScale(1.0);
        m_window = std::make_unique<MainWindow>(*m_ui, *m_settings);
        m_window->resize(1600, 1000);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_window->startup(QString());
        m_window->resize(1600, 1000);
        QTest::qWait(100);
        m_normalHeight = m_window->ribbon().height();
        QVERIFY(m_normalHeight > 0);
    }

    void cleanupTestCase()
    {
        m_window->document().markSaved();
        m_window.reset();
    }

    void slimRibbonWhileSettingsStayOpen()
    {
        TouchSlider* slider = openSettings();
        QVERIFY(slider);
        QCOMPARE(slider->value(), 1.0);
        const int menuButton = m_window->popoverHost().current()->findChild<TouchButton*>()->sizeHint().height();

        drag(slider, 0.0);
        QCOMPARE(slider->value(), Ribbon::kMinScale);
        Ribbon& ribbon = m_window->ribbon();
        QCOMPARE(ribbon.scale(), Ribbon::kMinScale);
        QVERIFY2(ribbon.height() < m_normalHeight * 0.7,
                 qPrintable(QStringLiteral("%1 vs %2").arg(ribbon.height()).arg(m_normalHeight)));
        QVERIFY(ribbon.penButton()->sizeHint().height() < ribbon.height());
        // The menu is still open, still at the interface size, and saved setting follows.
        QCOMPARE(m_window->popoverHost().currentKey(), QStringLiteral("settings"));
        QCOMPARE(m_window->popoverHost().current()->findChild<TouchButton*>()->sizeHint().height(), menuButton);
        QCOMPARE(m_ui->theme.uiScale(), 1.0);
        QCOMPARE(m_settings->ribbonScale(), Ribbon::kMinScale);
        shot(QStringLiteral("ribbon-slim"));
    }

    void biggerRibbonFitsTheWindow()
    {
        TouchSlider* slider = openSettings();
        QVERIFY(slider);
        drag(slider, 1.0);
        Ribbon& ribbon = m_window->ribbon();
        QCOMPARE(ribbon.scale(), Ribbon::kMaxScale);
        QVERIFY(ribbon.height() > m_normalHeight * 1.3);
        // Optional groups make room; More, the tools and Undo always stay.
        QVERIFY(ribbon.moreButton()->isVisible());
        QVERIFY(ribbon.penButton()->isVisible());
        QVERIFY(ribbon.eraserButton()->isVisible());
        QVERIFY(ribbon.pageButton()->isVisible());
        QCOMPARE(m_settings->ribbonScale(), Ribbon::kMaxScale);
        shot(QStringLiteral("ribbon-big"));
    }

    void keptWhenTheInterfaceSizeChanges()
    {
        m_window->popovers().close();
        m_window->applyUiScale(1.2);
        QTest::qWait(100);
        QCOMPARE(m_window->ribbon().scale(), Ribbon::kMaxScale);
        m_window->applyUiScale(1.0);
        QTest::qWait(100);
    }

    void normalSizeButton()
    {
        TouchSlider* slider = openSettings();
        QVERIFY(slider);
        auto* normal = m_window->popoverHost().current()->findChild<TouchButton*>(QStringLiteral("normalRibbon"));
        QVERIFY(normal);
        normal->click();
        QTest::qWait(100);
        QCOMPARE(slider->value(), 1.0);
        QCOMPARE(m_window->ribbon().scale(), 1.0);
        QCOMPARE(m_window->ribbon().height(), m_normalHeight);
        QCOMPARE(m_settings->ribbonScale(), 1.0);
    }

    void storedBetweenSessions()
    {
        m_settings->setRibbonScale(0.8);
        AppSettings reloaded;
        QCOMPARE(reloaded.ribbonScale(), 0.8);
        m_settings->setRibbonScale(5.0);
        QCOMPARE(m_settings->ribbonScale(), Ribbon::kMaxScale);
        m_settings->setRibbonScale(1.0);
    }
};

QTEST_MAIN(TestRibbonSize)
#include "tst_ribbon_size.moc"
