// The calculator on the real window: opening it from MORE, tapping keys, Ans, DEG/RAD, errors,
// "Put on board" (one undo step) and keyboard input. Set CLASSBOARD_SCREENSHOTS=<dir> for
// screenshots.
#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/PageOperations.h"
#include "math/Calculator.h"
#include "math/equation/EquationObject.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/UiContext.h"
#include "ui/popovers/CalculatorPanel.h"
#include "ui/widgets/TouchButton.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"

#include <QLabel>
#include <QLineEdit>
#include <QtTest>

using namespace cb;

class TestCalculatorUi : public QObject
{
    Q_OBJECT
private:
    std::unique_ptr<UiContext> m_ui;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<MainWindow> m_window;
    QString m_shots;

    Document& doc() { return m_window->document(); }
    Page& page() { return *doc().currentPage(); }

    void shot(const QString& name)
    {
        if (m_shots.isEmpty())
            return;
        QTest::qWait(250);
        m_window->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    CalculatorPanel* openCalculator()
    {
        m_window->popovers().close();
        QTest::qWait(100);
        QTest::mouseClick(m_window->ribbon().moreButton(), Qt::LeftButton);
        QTest::qWait(200);
        Popover* more = m_window->popoverHost().current();
        if (!more)
            return nullptr;
        TouchButton* tile = nullptr;
        for (TouchButton* b : more->findChildren<TouchButton*>())
            if (b->text() == QLatin1String("Calculator"))
                tile = b;
        if (!tile)
            return nullptr;
        tile->click();
        QTest::qWait(200);
        Popover* p = m_window->popoverHost().current();
        return p ? p->findChild<CalculatorPanel*>() : nullptr;
    }

    void tap(CalculatorPanel* calc, const QStringList& keys)
    {
        for (const QString& k : keys) {
            auto* b = calc->findChild<TouchButton*>(QStringLiteral("key:") + k);
            QVERIFY2(b, qPrintable(QStringLiteral("no key ") + k));
            b->click();
        }
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
        m_window = std::make_unique<MainWindow>(*m_ui, *m_settings);
        m_window->resize(1600, 1000);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_window->startup(QString());
        m_window->resize(1600, 1000);
        QTest::qWait(100);
        QVERIFY(m_ui->icons.exists(QStringLiteral("calculator")));
    }

    void cleanupTestCase()
    {
        doc().markSaved();
        m_window.reset();
    }

    void opensFromMoreAndCalculates()
    {
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        QCOMPARE(m_window->popoverHost().currentKey(), QStringLiteral("calculator"));
        tap(calc, {QStringLiteral("AC"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("×"), QStringLiteral("7")});
        QCOMPARE(calc->input(), QStringLiteral("12×7"));
        QCOMPARE(calc->resultText(), QStringLiteral("= 84")); // live preview while typing
        tap(calc, {QStringLiteral("=")});
        QCOMPARE(calc->resultText(), QStringLiteral("= 84"));
        auto* first = calc->findChild<TouchButton*>(QStringLiteral("history:0"));
        QVERIFY(first);
        QCOMPARE(first->text(), QStringLiteral("12×7  =  84"));
        shot(QStringLiteral("calculator"));
        // An operator right after "=" continues with the answer ...
        tap(calc, {QStringLiteral("÷"), QStringLiteral("4"), QStringLiteral("=")});
        QCOMPARE(calc->input(), QStringLiteral("Ans÷4"));
        QCOMPARE(calc->resultText(), QStringLiteral("= 21"));
        // ... a number starts a new calculation.
        tap(calc, {QStringLiteral("5")});
        QCOMPARE(calc->input(), QStringLiteral("5"));
        // A second operator replaces the first; minus after × stays (negative number).
        tap(calc, {QStringLiteral("+"), QStringLiteral("×"), QStringLiteral("−"), QStringLiteral("2"), QStringLiteral("=")});
        QCOMPARE(calc->input(), QStringLiteral("5×−2"));
        QCOMPARE(calc->resultText(), QStringLiteral("= −10"));
    }

    void scientificKeys()
    {
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        tap(calc, {QStringLiteral("AC"), QStringLiteral("sin"), QStringLiteral("3"), QStringLiteral("0"), QStringLiteral("=")});
        QCOMPARE(calc->input(), QStringLiteral("sin(30"));
        QCOMPARE(calc->resultText(), QStringLiteral("= 0.5")); // DEG by default, bracket closed for you
        tap(calc, {QStringLiteral("AC"), QStringLiteral("√"), QStringLiteral("1"), QStringLiteral("6"), QStringLiteral(")"),
                   QStringLiteral("+"), QStringLiteral("3"), QStringLiteral("x²"), QStringLiteral("+"),
                   QStringLiteral("4"), QStringLiteral("n!"), QStringLiteral("=")});
        QCOMPARE(calc->input(), QStringLiteral("√(16)+3²+4!"));
        QCOMPARE(calc->resultText(), QStringLiteral("= 37"));
        // Backspace removes a function name in one tap.
        tap(calc, {QStringLiteral("AC"), QStringLiteral("2"), QStringLiteral("×"), QStringLiteral("cos⁻¹")});
        QCOMPARE(calc->input(), QStringLiteral("2×acos("));
        tap(calc, {QStringLiteral("⌫")});
        QCOMPARE(calc->input(), QStringLiteral("2×"));
        // RAD mode.
        const auto rad = calc->findChildren<TouchButton*>();
        TouchButton* radButton = nullptr;
        for (TouchButton* b : rad)
            if (b->text() == QLatin1String("RAD"))
                radButton = b;
        QVERIFY(radButton);
        radButton->click();
        tap(calc, {QStringLiteral("AC"), QStringLiteral("cos"), QStringLiteral("π"), QStringLiteral("=")});
        QCOMPARE(calc->resultText(), QStringLiteral("= −1"));
        QCOMPARE(m_window->calculator().angleUnit(), math::Calculator::AngleUnit::Radians);
        m_window->calculator().setAngleUnit(math::Calculator::AngleUnit::Degrees);
    }

    void errorsAreShownAndChangeNothing()
    {
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        const int before = m_window->calculator().history().size();
        tap(calc, {QStringLiteral("AC"), QStringLiteral("1"), QStringLiteral("÷"), QStringLiteral("0")});
        QCOMPARE(calc->resultText(), QString()); // no nagging while typing
        tap(calc, {QStringLiteral("=")});
        QVERIFY(calc->resultText().contains(QStringLiteral("division by zero")));
        QCOMPARE(m_window->calculator().history().size(), before);
    }

    void putOnBoardAddsOneEquation()
    {
        pageops::clearPage(doc(), doc().currentPageIndex());
        const int objects = page().objectCount();
        const int undoIndex = doc().commands().index();
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        tap(calc, {QStringLiteral("AC"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("×"), QStringLiteral("7"),
                   QStringLiteral("=")});
        auto* board = static_cast<TouchButton*>(nullptr);
        for (TouchButton* b : calc->findChildren<TouchButton*>())
            if (b->text() == QLatin1String("Put on board"))
                board = b;
        QVERIFY(board);
        board->click();
        QTest::qWait(150);
        QCOMPARE(page().objectCount(), objects + 1);
        const DocumentObject* o = page().objectAt(page().objectCount() - 1);
        QCOMPARE(o->type(), ObjectType::Equation);
        QCOMPARE(static_cast<const EquationObject*>(o)->latex(), QStringLiteral("12 \\times 7 = 84"));
        QCOMPARE(doc().commands().index(), undoIndex + 1);
        QVERIFY(m_window->popoverHost().currentKey().isEmpty()); // the board is visible again
        shot(QStringLiteral("calculator-on-board"));
        doc().commands().undo();
        QCOMPARE(page().objectCount(), objects);
        // The calculator remembers its state: input, Ans and history.
        calc = openCalculator();
        QVERIFY(calc);
        QCOMPARE(calc->input(), QStringLiteral("12×7"));
        QCOMPARE(m_window->calculator().ans(), 84.0);
        QVERIFY(calc->findChild<TouchButton*>(QStringLiteral("history:0")));
        // An Ans calculation is written with the number, not the word "Ans".
        tap(calc, {QStringLiteral("÷"), QStringLiteral("4"), QStringLiteral("=")});
        for (TouchButton* b : calc->findChildren<TouchButton*>())
            if (b->text() == QLatin1String("Put on board"))
                board = b;
        board->click();
        QTest::qWait(150);
        QCOMPARE(static_cast<const EquationObject*>(page().objectAt(page().objectCount() - 1))->latex(),
                 QStringLiteral("84 \\div 4 = 21"));
        doc().commands().undo();
    }

    void keyboardTyping()
    {
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        calc->press(QStringLiteral("AC"));
        auto* input = calc->findChild<QLineEdit*>(QStringLiteral("calcInput"));
        QVERIFY(input);
        QVERIFY(input->isReadOnly()); // no on-screen touch keyboard pops up on the board
        input->setFocus();
        QTest::keyClicks(input, QStringLiteral("2*(3+4)-5/5"));
        QCOMPARE(calc->input(), QStringLiteral("2×(3+4)−5÷5"));
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(calc->resultText(), QStringLiteral("= 13"));
        QTest::keyClick(input, Qt::Key_Backspace); // edit the calculation after "="
        QCOMPARE(calc->input(), QStringLiteral("2×(3+4)−5÷"));
        QTest::keyClicks(input, QStringLiteral("1="));
        QCOMPARE(calc->resultText(), QStringLiteral("= 9"));
        m_window->popovers().close();
    }

    void keysDoNotTriggerBoardShortcuts()
    {
        // A selected object on the board and the pen active: typing in the calculator must not
        // delete the object (Backspace / Delete) or switch tools (P, E, V, T shortcuts).
        pageops::clearPage(doc(), doc().currentPageIndex());
        auto eq = EquationObject::create(QStringLiteral("x"), m_window->canvas().viewCenterInPage(), 40, Qt::white);
        const ObjectId id = eq->id();
        std::vector<ObjectPtr> v;
        v.push_back(std::move(eq));
        doc().commands().push(std::make_unique<AddObjectsCommand>(page().id(), std::move(v)));
        m_window->activateTool(ToolId::Pen);
        m_window->canvas().selectionModel().setSingle(id);
        CalculatorPanel* calc = openCalculator();
        QVERIFY(calc);
        calc->press(QStringLiteral("AC"));
        auto* input = calc->findChild<QLineEdit*>(QStringLiteral("calcInput"));
        input->setFocus();
        QTest::keyClicks(input, QStringLiteral("2e"));
        QTest::keyClick(input, Qt::Key_Backspace);
        QTest::keyClick(input, Qt::Key_Delete);
        QTest::keyClicks(input, QStringLiteral("pvt"));
        QVERIFY(page().object(id));                                         // not deleted
        QCOMPARE(m_window->canvas().tools().activeTool()->id(), ToolId::Pen); // no tool switch
        QCOMPARE(calc->input(), QStringLiteral("2pvt"));
        QTest::keyClick(input, Qt::Key_Return);
        QVERIFY(!calc->resultText().isEmpty()); // explains that p, v, t are unknown
        m_window->popovers().close();
    }
};

QTEST_MAIN(TestCalculatorUi)
#include "tst_calculator_ui.moc"
