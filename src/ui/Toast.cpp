#include "ui/Toast.h"

#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace cb {

// ============================================================================================ Toast

Toast::Toast(const UiContext& ui, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
{
    const Theme& t = ui.theme;
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(t.dpi(20), t.dpi(10), t.dpi(10), t.dpi(10));
    layout->setSpacing(t.dpi(10));
    m_label = new QLabel(this);
    m_label->setFont(t.font());
    m_label->setStyleSheet(QStringLiteral("color: %1;").arg(t.color(ThemeColor::Text).name()));
    m_label->setWordWrap(false);
    layout->addWidget(m_label, 1);
    m_actions = new QHBoxLayout();
    m_actions->setSpacing(t.dpi(8));
    layout->addLayout(m_actions);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Toast::dismiss);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    hide();
    if (parent)
        parent->installEventFilter(this);
}

void Toast::clearActions()
{
    for (TouchButton* b : m_buttons)
        b->deleteLater();
    m_buttons.clear();
}

void Toast::showMessage(const QString& text, int milliseconds)
{
    clearActions();
    m_progress = -1;
    m_label->setText(text);
    present(milliseconds);
}

void Toast::showProgress(const QString& text, int percent)
{
    if (m_progress < 0 || !isVisible())
        clearActions();
    m_progress = percent;
    m_label->setText(percent >= 0 ? QStringLiteral("%1  %2%").arg(text).arg(percent) : text);
    present(0);
}

void Toast::showActions(const QString& text, const QVector<Action>& actions, int milliseconds)
{
    clearActions();
    m_progress = -1;
    m_label->setText(text);
    for (const Action& a : actions) {
        auto* b = new TouchButton(m_ui, QString(), a.label, TouchButton::Style::Pill, this);
        b->setPrimary(a.primary);
        const auto cb = a.callback;
        connect(b, &QAbstractButton::clicked, this, [this, cb]() {
            dismiss();
            if (cb)
                cb();
        });
        m_actions->addWidget(b);
        m_buttons.push_back(b);
    }
    present(milliseconds);
}

void Toast::present(int milliseconds)
{
    adjustSize();
    reposition();
    show();
    raise();
    update();
    if (milliseconds > 0)
        m_timer.start(milliseconds);
    else
        m_timer.stop();
}

void Toast::dismiss()
{
    m_timer.stop();
    m_progress = -1;
    hide();
}

void Toast::reposition()
{
    if (!parentWidget())
        return;
    const QSize s = sizeHint().expandedTo(QSize(m_ui.theme.dpi(280), m_ui.theme.dpi(56)));
    const int w = std::min(s.width(), parentWidget()->width() - m_ui.theme.dpi(32));
    resize(w, s.height());
    move((parentWidget()->width() - w) / 2, m_ui.theme.dpi(16));
}

bool Toast::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible())
        reposition();
    return QWidget::eventFilter(watched, event);
}

void Toast::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal radius = r.height() / 2;
    p.setPen(QPen(t.color(ThemeColor::PopoverBorder), t.dp(1)));
    p.setBrush(t.color(ThemeColor::Popover));
    p.drawRoundedRect(r, radius, radius);
    if (m_progress >= 0) {
        QRectF bar(r.left() + radius, r.bottom() - t.dp(6), (r.width() - 2 * radius) * m_progress / 100.0, t.dp(3));
        p.setPen(Qt::NoPen);
        p.setBrush(t.color(ThemeColor::Accent));
        p.drawRoundedRect(bar, bar.height() / 2, bar.height() / 2);
    }
}

// =================================================================================== ConfirmOverlay

ConfirmOverlay::ConfirmOverlay(const UiContext& ui, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
{
    setGeometry(parent->rect());
    parent->installEventFilter(this);
}

ConfirmOverlay* ConfirmOverlay::ask(const UiContext& ui, QWidget* parent, const QString& title, const QString& message,
                                    const QVector<Choice>& choices)
{
    const Theme& t = ui.theme;
    auto* overlay = new ConfirmOverlay(ui, parent);
    auto* card = new QWidget(overlay);
    overlay->m_card = card;
    card->setObjectName(QStringLiteral("confirmCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(QStringLiteral("#confirmCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
                            .arg(t.color(ThemeColor::Popover).name(), t.color(ThemeColor::PopoverBorder).name())
                            .arg(t.dpi(18)));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(t.dpi(28), t.dpi(24), t.dpi(28), t.dpi(22));
    layout->setSpacing(t.dpi(12));
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setFont(t.font(t.metric(ThemeMetric::TitleFontSize) + 3, true));
    titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(t.color(ThemeColor::Text).name()));
    auto* messageLabel = new QLabel(message, card);
    messageLabel->setWordWrap(true);
    messageLabel->setFont(t.font());
    messageLabel->setStyleSheet(QStringLiteral("color: %1;").arg(t.color(ThemeColor::TextMuted).name()));
    layout->addWidget(titleLabel);
    layout->addWidget(messageLabel);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(t.dpi(10));
    buttons->addStretch(1);
    for (const Choice& c : choices) {
        auto* b = new TouchButton(ui, QString(), c.label, TouchButton::Style::Pill, card);
        b->setPrimary(c.primary);
        b->setDanger(c.danger);
        const auto cb = c.callback;
        QObject::connect(b, &QAbstractButton::clicked, overlay, [overlay, cb]() {
            overlay->hide();
            overlay->deleteLater();
            if (cb)
                cb();
        });
        buttons->addWidget(b);
    }
    layout->addLayout(buttons);
    const int w = std::min(t.dpi(560), parent->width() - t.dpi(40));
    card->setFixedWidth(w);
    card->adjustSize();
    card->move((parent->width() - card->width()) / 2, (parent->height() - card->height()) / 2);
    overlay->show();
    overlay->raise();
    return overlay;
}

bool ConfirmOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        if (m_card)
            m_card->move((width() - m_card->width()) / 2, (height() - m_card->height()) / 2);
    }
    return QWidget::eventFilter(watched, event);
}

void ConfirmOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_ui.theme.color(ThemeColor::Scrim));
}

void ConfirmOverlay::mousePressEvent(QMouseEvent* event)
{
    if (m_card && !m_card->geometry().contains(event->pos())) {
        hide();
        deleteLater();
    }
    event->accept();
}

} // namespace cb
