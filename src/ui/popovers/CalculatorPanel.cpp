#include "ui/popovers/CalculatorPanel.h"

#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "math/Calculator.h"
#include "math/equation/EquationObject.h"
#include "tools/SelectionModel.h"
#include "tools/ToolSettings.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/TouchButton.h"

#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>

namespace cb {

namespace {

const QString kMinus = QStringLiteral("−");

// Text a key adds. Functions open their bracket; it is closed automatically if forgotten.
QString insertionFor(const QString& key)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("sin"), QStringLiteral("sin(")},   {QStringLiteral("cos"), QStringLiteral("cos(")},
        {QStringLiteral("tan"), QStringLiteral("tan(")},   {QStringLiteral("ln"), QStringLiteral("ln(")},
        {QStringLiteral("log"), QStringLiteral("log(")},   {QStringLiteral("sin⁻¹"), QStringLiteral("asin(")},
        {QStringLiteral("cos⁻¹"), QStringLiteral("acos(")}, {QStringLiteral("tan⁻¹"), QStringLiteral("atan(")},
        {QStringLiteral("√"), QStringLiteral("√(")},       {QStringLiteral("x²"), QStringLiteral("²")},
        {QStringLiteral("xʸ"), QStringLiteral("^")},       {QStringLiteral("n!"), QStringLiteral("!")},
    };
    return map.value(key, key);
}

// Keys that continue the previous result when pressed right after "=" (Ans × …).
bool continuesResult(const QString& text)
{
    static const QStringList ops = {QStringLiteral("+"), QStringLiteral("−"), QStringLiteral("×"), QStringLiteral("÷"),
                                    QStringLiteral("^"), QStringLiteral("²"), QStringLiteral("!"), QStringLiteral("%")};
    return ops.contains(text);
}

bool isBinaryOperator(QChar c)
{
    return c == QLatin1Char('+') || c == QChar(0x2212) || c == QLatin1Char('-') || c == QChar(0x00D7)
        || c == QChar(0x00F7) || c == QLatin1Char('*') || c == QLatin1Char('/') || c == QLatin1Char('^');
}

} // namespace

const QVector<QStringList>& CalculatorPanel::keys()
{
    static const QVector<QStringList> rows = {
        {QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"), QStringLiteral("ln"), QStringLiteral("log"),
         QStringLiteral("π")},
        {QStringLiteral("sin⁻¹"), QStringLiteral("cos⁻¹"), QStringLiteral("tan⁻¹"), QStringLiteral("√"),
         QStringLiteral("x²"), QStringLiteral("xʸ")},
        {QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("÷"), QStringLiteral("⌫"),
         QStringLiteral("AC")},
        {QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("×"), QStringLiteral("("),
         QStringLiteral(")")},
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), kMinus, QStringLiteral("n!"), QStringLiteral("%")},
        {QStringLiteral("0"), QStringLiteral("."), QStringLiteral("e"), QStringLiteral("+"), QStringLiteral("Ans"),
         QStringLiteral("=")},
    };
    return rows;
}

CalculatorPanel::CalculatorPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
{
    const UiContext& ui = s.ui;
    const Theme& t = ui.theme;
    auto* layout = panel::makeLayout(ui, this);

    // Display: what is typed, and the result underneath.
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("calcInput"));
    m_input->setReadOnly(true); // keys and keyboard go through press(); no touch keyboard pops up
    m_input->setAlignment(Qt::AlignRight);
    // Sizes through the style sheet: the application style sheet would override setFont().
    m_input->setStyleSheet(QStringLiteral("QLineEdit { font-size: %1px; }").arg(t.dpi(30)));
    m_input->setMinimumHeight(t.dpi(60));
    m_input->setPlaceholderText(tr("Type or tap a calculation"));
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_result = new QLabel(this);
    m_result->setObjectName(QStringLiteral("calcResult"));
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setMinimumHeight(t.dpi(56));
    m_result->setWordWrap(true);
    layout->addWidget(m_result);

    // Angle unit.
    const bool degrees = s.calculator.angleUnit() == math::Calculator::AngleUnit::Degrees;
    QVector<TouchButton*> unitButtons;
    auto* units = panel::choices(ui, {tr("DEG"), tr("RAD")}, degrees ? 0 : 1, this,
                                 [this](int i) {
                                     m_s.calculator.setAngleUnit(i == 0 ? math::Calculator::AngleUnit::Degrees
                                                                        : math::Calculator::AngleUnit::Radians);
                                     updatePreview();
                                 },
                                 2, &unitButtons);
    m_deg = unitButtons.value(0);
    m_rad = unitButtons.value(1);
    for (TouchButton* b : unitButtons)
        b->setToolTip(tr("Angles in degrees (DEG) or radians (RAD) for sin, cos, tan and their inverses"));
    auto* unitHint = panel::hint(ui, tr("Angles"), this);
    layout->addWidget(panel::row(ui, {unitHint, units}, this));

    // Keys.
    auto* keysWidget = new QWidget(this);
    auto* grid = new QGridLayout(keysWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(t.dpi(6));
    for (int r = 0; r < keys().size(); ++r) {
        for (int c = 0; c < keys()[r].size(); ++c) {
            const QString key = keys()[r][c];
            auto* b = new TouchButton(ui, QString(), key, TouchButton::Style::Pill, keysWidget);
            b->setObjectName(QStringLiteral("key:") + key);
            b->setMinimumSize(t.dpi(60), t.dpi(50));
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setFocusPolicy(Qt::NoFocus); // keep keyboard focus in the display
            if (key == QLatin1String("="))
                b->setPrimary(true);
            if (key == QLatin1String("AC"))
                b->setDanger(true);
            connect(b, &QAbstractButton::clicked, this, [this, key]() { press(key); });
            grid->addWidget(b, r, c);
        }
    }
    layout->addWidget(keysWidget);

    // Actions.
    auto* board = panel::pill(ui, QStringLiteral("equation"), tr("Put on board"), this, [this]() { putOnBoard(); });
    board->setToolTip(tr("Writes the calculation and its result on the page as an equation"));
    auto* copy = panel::pill(ui, QStringLiteral("copy"), tr("Copy result"), this, [this]() {
        const math::Calculator::Result r = m_s.calculator.evaluate(m_input->text());
        if (r.ok)
            QGuiApplication::clipboard()->setText(math::Calculator::toInput(r.value));
    });
    layout->addWidget(panel::row(ui, {board, copy}, this));

    // Recent calculations: tap one to use its result.
    layout->addWidget(panel::section(ui, tr("Recent"), this));
    m_history = new QWidget(this);
    auto* historyLayout = new QVBoxLayout(m_history);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(t.dpi(2));
    layout->addWidget(m_history);

    m_input->setText(s.calculator.input());
    m_input->setCursorPosition(m_input->text().size());
    connect(m_input, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_s.calculator.setInput(text);
        updatePreview();
    });
    // Reopened right after "=": show that result and keep "=" behaviour for the next key.
    m_justEvaluated = s.calculator.showingResult() && !s.calculator.history().isEmpty();
    if (m_justEvaluated)
        showResult(s.calculator.history().first().result);
    else
        updatePreview();
    rebuildHistory();
    m_input->setFocus();
}

QString CalculatorPanel::input() const { return m_input->text(); }

QString CalculatorPanel::resultText() const { return m_result->text(); }

void CalculatorPanel::press(const QString& key)
{
    if (key == QLatin1String("="))
        equals();
    else if (key == QLatin1String("AC"))
        clearAll();
    else if (key == QStringLiteral("⌫"))
        backspace();
    else
        insert(insertionFor(key));
}

void CalculatorPanel::setJustEvaluated(bool on)
{
    m_justEvaluated = on;
    m_s.calculator.setShowingResult(on);
}

void CalculatorPanel::showResult(const QString& text)
{
    const Theme& t = m_s.ui.theme;
    m_result->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; font-weight: 600;")
                                .arg(t.color(ThemeColor::Accent).name())
                                .arg(t.dpi(40)));
    m_result->setText(QStringLiteral("= ") + text);
}

void CalculatorPanel::setInput(const QString& text, int cursor)
{
    m_input->setText(text);
    m_input->setCursorPosition(cursor < 0 ? text.size() : cursor);
}

void CalculatorPanel::insert(const QString& text)
{
    QString current = m_input->text();
    int cursor = m_input->cursorPosition();
    if (m_justEvaluated) {
        // After "=": an operator continues with the result, anything else starts afresh.
        current = continuesResult(text) ? QStringLiteral("Ans") : QString();
        cursor = current.size();
        setJustEvaluated(false);
    } else if (current.isEmpty() && continuesResult(text) && text != kMinus && !m_s.calculator.history().isEmpty()) {
        current = QStringLiteral("Ans");
        cursor = current.size();
    }
    // A second operator replaces the first (3 + × 4 → 3 × 4), but "3 × −2" stays possible.
    if (text.size() == 1 && isBinaryOperator(text[0]) && text != kMinus) {
        int before = cursor - 1;
        while (before >= 0 && current[before].isSpace())
            --before;
        if (before >= 0 && isBinaryOperator(current[before])) {
            current.remove(before, cursor - before);
            cursor = before;
        }
    }
    current.insert(cursor, text);
    setInput(current, cursor + text.size());
}

void CalculatorPanel::backspace()
{
    setJustEvaluated(false);
    QString current = m_input->text();
    const int cursor = m_input->cursorPosition();
    if (cursor <= 0)
        return;
    static const QStringList tokens = {QStringLiteral("asin("), QStringLiteral("acos("), QStringLiteral("atan("),
                                       QStringLiteral("sin("),  QStringLiteral("cos("),  QStringLiteral("tan("),
                                       QStringLiteral("ln("),   QStringLiteral("log("),  QStringLiteral("√("),
                                       QStringLiteral("Ans")};
    const QString before = current.left(cursor);
    int remove = 1;
    for (const QString& token : tokens) {
        if (before.endsWith(token)) {
            remove = token.size();
            break;
        }
    }
    current.remove(cursor - remove, remove);
    setInput(current, cursor - remove);
}

void CalculatorPanel::clearAll()
{
    setJustEvaluated(false);
    setInput(QString());
    m_result->clear();
}

void CalculatorPanel::equals()
{
    const math::Calculator::Result r = m_s.calculator.calculate(m_input->text());
    const Theme& t = m_s.ui.theme;
    if (r.ok) {
        showResult(r.text);
        setJustEvaluated(true);
        rebuildHistory();
    } else {
        m_result->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                    .arg(t.color(ThemeColor::Danger).name())
                                    .arg(t.dpi(16)));
        m_result->setText(r.error);
    }
}

void CalculatorPanel::updatePreview()
{
    if (m_justEvaluated)
        return;
    const Theme& t = m_s.ui.theme;
    // While typing, a quiet preview; errors only show once "=" is pressed.
    const math::Calculator::Result r = m_s.calculator.evaluate(m_input->text());
    m_result->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                .arg(t.color(ThemeColor::TextMuted).name())
                                .arg(t.dpi(30)));
    m_result->setText(r.ok ? QStringLiteral("= ") + r.text : QString());
}

void CalculatorPanel::rebuildHistory()
{
    QLayout* l = m_history->layout();
    while (QLayoutItem* item = l->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const auto& history = m_s.calculator.history();
    if (history.isEmpty()) {
        l->addWidget(panel::hint(m_s.ui, tr("Your calculations appear here. Tap one to use its result."), m_history));
        return;
    }
    const int shown = std::min(4, static_cast<int>(history.size()));
    for (int i = 0; i < shown; ++i) {
        const math::Calculator::Entry e = history[i];
        auto* b = new TouchButton(m_s.ui, QString(), e.expression + QStringLiteral("  =  ") + e.result,
                                  TouchButton::Style::Row, m_history);
        b->setObjectName(QStringLiteral("history:%1").arg(i));
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(tr("Use this result"));
        const double value = e.value;
        connect(b, &QAbstractButton::clicked, this, [this, value]() {
            insert(value < 0 ? QLatin1Char('(') + math::Calculator::toInput(value) + QLatin1Char(')')
                             : math::Calculator::toInput(value));
        });
        l->addWidget(b);
    }
}

void CalculatorPanel::putOnBoard()
{
    Page* page = m_s.doc.currentPage();
    if (!page)
        return;
    QString expression;
    QString result;
    const auto& history = m_s.calculator.history();
    if (m_justEvaluated && !history.isEmpty()) {
        expression = history.first().expression;
        result = history.first().result;
    } else {
        const math::Calculator::Result r = m_s.calculator.evaluate(m_input->text());
        if (!r.ok) {
            m_s.toast.showMessage(m_input->text().trimmed().isEmpty() ? tr("Type a calculation first.") : r.error, 3000);
            return;
        }
        expression = math::Calculator::substituteAns(m_input->text().trimmed(), m_s.calculator.ans());
        result = r.text;
    }
    const QString latex = math::Calculator::toLatex(expression, result);
    auto eq = EquationObject::create(latex, m_s.canvas.viewCenterInPage(), 56.0, m_s.tools.penColor());
    const ObjectId id = eq->id();
    std::vector<ObjectPtr> objects;
    objects.push_back(std::move(eq));
    m_s.doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), tr("Put calculation on board")));
    m_s.activateTool(ToolId::Select);
    m_s.canvas.selectionModel().setSingle(id);
    m_s.popovers.close();
}

bool CalculatorPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_input)
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::ShortcutOverride) {
        // While typing a calculation, keys belong to the calculator, not to the board's
        // shortcuts (Backspace / Delete would delete the selection, E would pick the eraser).
        auto* key = static_cast<QKeyEvent*>(event);
        const bool plainText = !key->text().isEmpty() && key->text()[0].isPrint()
            && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (plainText || key->key() == Qt::Key_Backspace || key->key() == Qt::Key_Delete
            || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            event->accept();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        equals();
        return true;
    case Qt::Key_Backspace:
        backspace();
        return true;
    case Qt::Key_Delete: {
        setJustEvaluated(false);
        QString current = m_input->text();
        const int cursor = m_input->cursorPosition();
        if (cursor < current.size()) {
            current.remove(cursor, 1);
            setInput(current, cursor);
        }
        return true;
    }
    case Qt::Key_Escape:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Tab:
        return false; // cursor movement, closing the popover
    default:
        break;
    }
    if (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::SelectAll))
        return false;
    if (key->matches(QKeySequence::Paste)) {
        insert(QGuiApplication::clipboard()->text().simplified());
        return true;
    }
    const QString text = key->text();
    if (text.isEmpty() || !text[0].isPrint())
        return false;
    for (QChar c : text) {
        if (c == QLatin1Char('='))
            equals();
        else if (c == QLatin1Char('*'))
            insert(QStringLiteral("×"));
        else if (c == QLatin1Char('/'))
            insert(QStringLiteral("÷"));
        else if (c == QLatin1Char('-'))
            insert(kMinus);
        else
            insert(QString(c));
    }
    return true;
}

} // namespace cb
