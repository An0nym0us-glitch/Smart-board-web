#include "ui/popovers/MagicEquationPanel.h"

#include "ai/MathInkRecognizer.h"
#include "ai/Recognition.h"
#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Document.h"
#include "math/equation/EquationObject.h"
#include "tools/MagicEquation.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QThread>

namespace cb {

namespace {
class Preview : public QWidget
{
public:
    Preview(const UiContext& ui, QWidget* parent)
        : QWidget(parent)
        , m_ui(ui)
    {
        setMinimumHeight(ui.theme.dpi(120));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setLatex(const QString& latex)
    {
        m_latex = latex;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Theme& t = m_ui.theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1f, 0x2b, 0x26));
        p.drawRoundedRect(QRectF(rect()), t.dp(12), t.dp(12));
        if (!m_latex.trimmed().isEmpty())
            EquationObject::paintFormula(p, m_latex, QRectF(rect()).adjusted(t.dp(12), t.dp(8), -t.dp(12), -t.dp(8)),
                                         t.dp(44), QColor(245, 245, 240));
    }

private:
    const UiContext& m_ui;
    QString m_latex;
};
} // namespace

MagicEquationPanel::MagicEquationPanel(const AppServices& s, const QVector<ObjectId>& strokes, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
    , m_strokes(strokes)
{
    const UiContext& ui = s.ui;
    auto* layout = panel::makeLayout(ui, this);

    // Working state.
    m_working = new QWidget(this);
    auto* wl = panel::makeLayout(ui, m_working);
    wl->addWidget(panel::hint(ui, tr("Recognising the selected handwriting… Your writing stays unchanged."), m_working));
    wl->addWidget(panel::row(ui, {panel::pill(ui, QStringLiteral("close"), tr("Cancel"), m_working, [this]() { cancel(); })}, m_working));
    layout->addWidget(m_working);

    // Result state: preview, confidence, Accept / Edit / Cancel.
    m_result = new QWidget(this);
    auto* rl = panel::makeLayout(ui, m_result);
    auto* preview = new Preview(ui, m_result);
    m_preview = preview;
    rl->addWidget(preview);
    m_confidenceLabel = panel::hint(ui, QString(), m_result);
    rl->addWidget(m_confidenceLabel);
    m_latex = new QLineEdit(m_result);
    m_latex->setMinimumHeight(ui.theme.dpi(44));
    m_latex->setVisible(false);
    connect(m_latex, &QLineEdit::textChanged, this, [preview](const QString& t) { preview->setLatex(t); });
    connect(m_latex, &QLineEdit::returnPressed, this, [this]() { accept(); });
    rl->addWidget(m_latex);
    auto* acceptButton = panel::pill(ui, QStringLiteral("check"), tr("Accept"), m_result, [this]() { accept(); }, true);
    acceptButton->setObjectName(QStringLiteral("accept"));
    auto* editButton = panel::pill(ui, QStringLiteral("edit"), tr("Edit"), m_result, [this]() { edit(); });
    auto* cancelButton = panel::pill(ui, QStringLiteral("close"), tr("Cancel"), m_result, [this]() { cancel(); });
    rl->addWidget(panel::row(ui, {acceptButton, editButton, cancelButton}, m_result));
    rl->addWidget(panel::hint(ui, tr("Accept replaces the handwriting with an editable equation (undo brings it back)."), m_result));
    layout->addWidget(m_result);

    // Failure state: Try again / Edit manually / Cancel.
    m_failure = new QWidget(this);
    auto* fl = panel::makeLayout(ui, m_failure);
    m_failureLabel = panel::hint(ui, QString(), m_failure);
    fl->addWidget(m_failureLabel);
    m_tryAgain = panel::pill(ui, QStringLiteral("reset"), tr("Try again"), m_failure, [this]() { tryAgain(); }, true);
    auto* manual = panel::pill(ui, QStringLiteral("equation"), tr("Edit manually"), m_failure, [this]() { editManually(); });
    auto* cancelFailure = panel::pill(ui, QStringLiteral("close"), tr("Cancel"), m_failure, [this]() { cancel(); });
    fl->addWidget(panel::row(ui, {m_tryAgain, manual, cancelFailure}, m_failure));
    layout->addWidget(m_failure);

    m_result->hide();
    m_failure->hide();
    startRecognition(false);
}

MagicEquationPanel::~MagicEquationPanel()
{
    // A pending recognition result is ignored after the panel is gone.
    ++m_generation;
}

QString MagicEquationPanel::latex() const
{
    return m_latex->text();
}

void MagicEquationPanel::setState(State state)
{
    m_state = state;
    m_working->setVisible(state == State::Recognizing);
    m_result->setVisible(state == State::Result);
    m_failure->setVisible(state == State::Failed);
    emit stateChanged(state);
}

void MagicEquationPanel::startRecognition(bool relaxed)
{
    Page* page = m_s.doc.currentPage();
    if (!page || !magic::isHandwriting(*page, m_strokes)) {
        showFailure(tr("Select handwriting with SELECT first (only pen strokes), then tap ✨ Magic Equation Maker."));
        return;
    }
    setState(State::Recognizing);
    const InkSample ink = magic::collectInk(*page, m_strokes);
    const EquationRecognizer* registered = m_s.recognizers.equationRecognizer();
    const int generation = ++m_generation;
    QPointer<MagicEquationPanel> self(this);
    // Recognition runs off the interface thread; the page is never touched here.
    auto* worker = QThread::create([self, generation, ink, registered, relaxed]() {
        QVector<EquationCandidate> candidates;
        if (relaxed) {
            MathInkRecognizer::Options options;
            options.relaxed = true;
            candidates = MathInkRecognizer(options).recognize(ink);
        } else if (registered) {
            candidates = registered->recognize(ink);
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, generation, candidates]() {
                if (!self || self->m_generation != generation || self->m_state != State::Recognizing)
                    return;
                if (candidates.isEmpty() || candidates.first().latex.trimmed().isEmpty())
                    self->showFailure(tr("The handwriting could not be recognised with confidence."));
                else
                    self->showResult(candidates.first().latex, candidates.first().confidence);
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MagicEquationPanel::showResult(const QString& latex, double confidence)
{
    m_confidence = confidence;
    m_latex->setText(latex);
    static_cast<Preview*>(m_preview)->setLatex(latex);
    const int percent = qRound(confidence * 100);
    m_confidenceLabel->setText(confidence < 0.42 ? tr("Best guess (confidence %1 %). Check it before accepting.").arg(percent)
                                                 : tr("Recognised (confidence %1 %).").arg(percent));
    setState(State::Result);
}

void MagicEquationPanel::showFailure(const QString& reason)
{
    m_failureLabel->setText(reason + QLatin1Char(' ')
                            + tr("Your handwriting has not been changed. Try again, write the equation in the Equation "
                                 "Making Centre, or cancel."));
    m_tryAgain->setEnabled(!m_relaxedTried);
    setState(State::Failed);
}

void MagicEquationPanel::accept()
{
    if (m_state != State::Result)
        return;
    Page* page = m_s.doc.currentPage();
    if (!page)
        return;
    const ObjectId id = magic::acceptEquation(m_s.doc, *page, m_strokes, latex());
    if (id.isNull()) {
        showFailure(tr("The handwriting is no longer on this page."));
        return;
    }
    setState(State::Accepted);
    m_s.activateTool(ToolId::Select);
    m_s.canvas.selectionModel().setSingle(id);
    m_s.toast.showActions(tr("Equation created. Double-tap it to edit."),
                          {{tr("Undo"), [sp = &m_s]() { sp->doc.commands().undo(); }, true}}, 5000);
    m_s.popovers.close();
}

void MagicEquationPanel::edit()
{
    if (m_state != State::Result)
        return;
    m_latex->setVisible(true);
    m_latex->setFocus();
    m_latex->selectAll();
}

void MagicEquationPanel::cancel()
{
    ++m_generation;
    setState(State::Cancelled);
    m_s.popovers.close();
}

void MagicEquationPanel::tryAgain()
{
    if (m_relaxedTried && m_state == State::Failed)
        return;
    m_relaxedTried = true;
    startRecognition(true);
}

void MagicEquationPanel::editManually()
{
    const QString guess = m_state == State::Result ? latex() : QString();
    ++m_generation;
    setState(State::Cancelled);
    m_s.popovers.setEquationPrefill(guess);
    const QRect canvasRect = m_s.canvas.rect();
    m_s.popovers.openAt(QStringLiteral("equation"), QRect(canvasRect.center() - QPoint(1, 1), QSize(2, 2)));
}

} // namespace cb
