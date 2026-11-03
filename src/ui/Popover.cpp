#include "ui/Popover.h"

#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScroller>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <algorithm>

namespace cb {

namespace {
qreal shadowMargin(const Theme& t) { return t.dp(16); }
} // namespace

// ============================================================================================ Popover

Popover::Popover(const UiContext& ui, const QString& key, const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
    , m_key(key)
{
    const Theme& t = ui.theme;
    setAttribute(Qt::WA_NoMousePropagation, true);

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(t.dpi(8));

    auto* header = new QHBoxLayout();
    header->setSpacing(t.dpi(6));
    m_back = new TouchButton(ui, QStringLiteral("back"), tr("Back"), TouchButton::Style::Icon, this);
    m_back->setFixedSize(t.dpi(44), t.dpi(44));
    m_back->setVisible(false);
    connect(m_back, &QAbstractButton::clicked, this, &Popover::backRequested);
    m_title = new QLabel(title, this);
    m_title->setFont(t.font(t.metric(ThemeMetric::TitleFontSize), true));
    QPalette pal = m_title->palette();
    pal.setColor(QPalette::WindowText, t.color(ThemeColor::PopoverHeader));
    m_title->setPalette(pal);
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(t.color(ThemeColor::PopoverHeader).name()));
    m_close = new TouchButton(ui, QStringLiteral("close"), tr("Close"), TouchButton::Style::Icon, this);
    m_close->setFixedSize(t.dpi(44), t.dpi(44));
    connect(m_close, &QAbstractButton::clicked, this, &Popover::closeRequested);
    header->addWidget(m_back);
    header->addWidget(m_title, 1);
    header->addWidget(m_close);
    m_layout->addLayout(header);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_scroll->setAutoFillBackground(false);
    m_layout->addWidget(m_scroll, 1);
    updateMargins();
}

void Popover::setTitle(const QString& title)
{
    m_title->setText(title);
}

void Popover::setContent(QWidget* content)
{
    m_content = content;
    content->setAutoFillBackground(false);
    m_scroll->setWidget(content);
    m_lastHint = content->sizeHint();
    content->installEventFilter(this);
}

bool Popover::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_content && event->type() == QEvent::LayoutRequest && !m_resizePending) {
        // Coalesce: re-place once the layout has settled, and only if the size really changed.
        m_resizePending = true;
        QTimer::singleShot(0, this, [this]() {
            m_resizePending = false;
            if (!m_content)
                return;
            const QSize hint = m_content->sizeHint();
            if (hint != m_lastHint) {
                m_lastHint = hint;
                emit contentResized();
            }
        });
    }
    return QWidget::eventFilter(watched, event);
}

void Popover::setBackVisible(bool visible)
{
    m_back->setVisible(visible);
}

void Popover::updateMargins()
{
    const Theme& t = m_ui.theme;
    const int s = qRound(shadowMargin(t));
    const int pad = t.scaledInt(ThemeMetric::PopoverPadding);
    const int notch = t.scaledInt(ThemeMetric::PopoverNotch);
    m_layout->setContentsMargins(s + pad, s + pad * 3 / 4 + (m_notchBelow ? 0 : notch), s + pad,
                                 s + pad + (m_notchBelow ? notch : 0));
}

void Popover::place(const QRect& anchor, const QRect& bounds)
{
    const Theme& t = m_ui.theme;
    const int s = qRound(shadowMargin(t));
    const int pad = t.scaledInt(ThemeMetric::PopoverPadding);
    const int notch = t.scaledInt(ThemeMetric::PopoverNotch);
    const int margin = t.dpi(8);
    const int headerH = t.dpi(48) + t.dpi(8);

    const QSize hint = m_content ? m_content->sizeHint() : QSize(0, 0);
    int innerW = std::max(hint.width(), t.dpi(m_minContentWidthDp)) + 2 * pad;
    innerW = std::min(innerW, bounds.width() - 2 * margin);
    const int wantedInnerH = headerH + hint.height() + pad * 7 / 4 + t.dpi(4);

    const int availAbove = anchor.top() - bounds.top() - margin - notch;
    const int availBelow = bounds.bottom() - anchor.bottom() - margin - notch;
    m_notchBelow = availAbove >= wantedInnerH || availAbove >= availBelow;
    const int avail = std::max(t.dpi(160), m_notchBelow ? availAbove : availBelow);
    const int innerH = std::min(wantedInnerH, avail);

    const bool scrolls = innerH < wantedInnerH;
    m_scroll->setVerticalScrollBarPolicy(scrolls ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (scrolls)
        QScroller::grabGesture(m_scroll->viewport(), QScroller::TouchGesture);
    else
        QScroller::ungrabGesture(m_scroll->viewport());

    const int w = innerW + 2 * s;
    const int h = innerH + 2 * s + notch;
    int x = anchor.center().x() - w / 2;
    x = std::clamp(x, bounds.left() + margin - s, std::max(bounds.left() + margin - s, bounds.right() - margin - w + s));
    int y;
    if (m_notchBelow) {
        const int tip = std::min(anchor.top() - t.dpi(2), bounds.bottom());
        y = tip - (s + innerH + notch);
    } else {
        const int tip = std::max(anchor.bottom() + t.dpi(2), bounds.top());
        y = tip - s;
    }
    const qreal radius = t.scaled(ThemeMetric::PopoverRadius);
    m_notchX = std::clamp<qreal>(anchor.center().x() - x, s + radius + notch, s + innerW - radius - notch);
    updateMargins();
    setGeometry(x, y, w, h);
    m_targetPos = QPoint(x, y);
    update();
}

QPainterPath Popover::bubblePath() const
{
    const Theme& t = m_ui.theme;
    const qreal s = shadowMargin(t);
    const qreal notch = t.scaled(ThemeMetric::PopoverNotch);
    const qreal radius = t.scaled(ThemeMetric::PopoverRadius);
    const QRectF bubble(s, s + (m_notchBelow ? 0 : notch), width() - 2 * s, height() - 2 * s - notch);
    QPainterPath path;
    path.addRoundedRect(bubble, radius, radius);
    QPainterPath tri;
    const qreal nw = notch * 1.25;
    if (m_notchBelow) {
        tri.moveTo(m_notchX - nw, bubble.bottom() - 1);
        tri.quadTo(m_notchX - nw * 0.35, bubble.bottom() + notch * 0.35, m_notchX - nw * 0.12, bubble.bottom() + notch * 0.9);
        tri.quadTo(m_notchX, bubble.bottom() + notch * 1.05, m_notchX + nw * 0.12, bubble.bottom() + notch * 0.9);
        tri.quadTo(m_notchX + nw * 0.35, bubble.bottom() + notch * 0.35, m_notchX + nw, bubble.bottom() - 1);
    } else {
        tri.moveTo(m_notchX - nw, bubble.top() + 1);
        tri.quadTo(m_notchX - nw * 0.35, bubble.top() - notch * 0.35, m_notchX - nw * 0.12, bubble.top() - notch * 0.9);
        tri.quadTo(m_notchX, bubble.top() - notch * 1.05, m_notchX + nw * 0.12, bubble.top() - notch * 0.9);
        tri.quadTo(m_notchX + nw * 0.35, bubble.top() - notch * 0.35, m_notchX + nw, bubble.top() + 1);
    }
    tri.closeSubpath();
    return path.united(tri);
}

void Popover::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPainterPath path = bubblePath();

    // Soft elevation shadow: concentric strokes of decreasing opacity, offset downwards.
    QColor shadow = t.color(ThemeColor::Shadow);
    p.save();
    p.translate(0, t.dp(3));
    p.setBrush(Qt::NoBrush);
    const int layers = 8;
    for (int i = layers; i >= 1; --i) {
        shadow.setAlphaF(0.035 + 0.01 * (layers - i));
        p.setPen(QPen(shadow, t.dp(1.8) * i, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    p.restore();

    p.setPen(QPen(t.color(ThemeColor::PopoverBorder), t.dp(1)));
    p.setBrush(t.color(ThemeColor::Popover));
    p.drawPath(path);
}

void Popover::mousePressEvent(QMouseEvent* event)
{
    event->accept();
}

void Popover::animateIn()
{
    const Theme& t = m_ui.theme;
    if (m_anim)
        m_anim->stop();
    auto* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);
    const QPoint target = m_targetPos;
    const int dy = (m_notchBelow ? 1 : -1) * t.dpi(10);
    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(150);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this, effect, target, dy](const QVariant& v) {
        const double f = v.toDouble();
        effect->setOpacity(f);
        move(target + QPoint(0, qRound(dy * (1.0 - f))));
    });
    connect(m_anim, &QVariantAnimation::finished, this, [this, target]() {
        move(target);
        setGraphicsEffect(nullptr);
        m_anim->deleteLater();
        m_anim = nullptr;
    });
    move(target + QPoint(0, dy));
    m_anim->start();
}

void Popover::animateOut(std::function<void()> done)
{
    if (m_anim) {
        m_anim->stop();
        m_anim->deleteLater();
        m_anim = nullptr;
    }
    auto* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(1.0);
    setGraphicsEffect(effect);
    auto* anim = new QVariantAnimation(this);
    anim->setDuration(90);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QVariantAnimation::valueChanged, this, [effect](const QVariant& v) { effect->setOpacity(v.toDouble()); });
    connect(anim, &QVariantAnimation::finished, this, [done = std::move(done)]() {
        if (done)
            done();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ======================================================================================== PopoverHost

PopoverHost::PopoverHost(const UiContext& ui, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    if (parent) {
        parent->installEventFilter(this);
        setGeometry(parent->rect());
    }
}

QRect PopoverHost::anchorRect() const
{
    if (m_anchor) {
        const QPoint global = m_anchor->mapToGlobal(QPoint(0, 0));
        return QRect(mapFromGlobal(global), m_anchor->size());
    }
    return m_anchorRect;
}

void PopoverHost::updateInteractive()
{
    const bool open = isOpen();
    setAttribute(Qt::WA_TransparentForMouseEvents, !open);
    if (open)
        raise();
}

void PopoverHost::show(Popover* popover, bool animate)
{
    popover->setParent(this);
    connect(popover, &Popover::closeRequested, this, &PopoverHost::closeAll, Qt::UniqueConnection);
    connect(popover, &Popover::backRequested, this, &PopoverHost::back, Qt::UniqueConnection);
    connect(popover, &Popover::contentResized, this, [this, popover]() {
        if (current() == popover)
            reposition();
    }, Qt::UniqueConnection);
    popover->place(anchorRect(), rect());
    popover->show();
    popover->raise();
    if (animate)
        popover->animateIn();
    updateInteractive();
    emit opened(popover->key());
}

void PopoverHost::open(Popover* popover, QWidget* anchor)
{
    closeAll();
    m_anchor = anchor;
    if (anchor)
        anchor->installEventFilter(this);
    m_stack.push_back(popover);
    show(popover, true);
}

void PopoverHost::openAt(Popover* popover, const QRect& anchorRect)
{
    closeAll();
    m_anchor = nullptr;
    m_anchorRect = anchorRect;
    m_stack.push_back(popover);
    show(popover, true);
}

void PopoverHost::push(Popover* popover)
{
    if (Popover* cur = current())
        cur->hide();
    popover->setBackVisible(true);
    m_stack.push_back(popover);
    show(popover, true);
}

void PopoverHost::back()
{
    if (m_stack.size() <= 1) {
        closeAll();
        return;
    }
    QPointer<Popover> top = m_stack.takeLast();
    if (top)
        top->deleteLater();
    if (Popover* prev = current()) {
        prev->place(anchorRect(), rect());
        prev->show();
        prev->animateIn();
        emit opened(prev->key());
    }
    updateInteractive();
}

void PopoverHost::replaceTop(Popover* popover)
{
    if (m_stack.isEmpty()) {
        popover->deleteLater();
        return;
    }
    QPointer<Popover> old = m_stack.takeLast();
    if (old) {
        old->hide();
        old->deleteLater();
    }
    popover->setBackVisible(!m_stack.isEmpty());
    m_stack.push_back(popover);
    show(popover, false);
}

void PopoverHost::closeAll()
{
    if (m_stack.isEmpty())
        return;
    const QString key = currentKey();
    const auto stack = m_stack;
    m_stack.clear();
    for (const QPointer<Popover>& p : stack) {
        if (!p)
            continue;
        if (p->isVisible()) {
            QPointer<Popover> guard = p;
            p->animateOut([guard]() {
                if (guard)
                    guard->deleteLater();
            });
        } else {
            p->deleteLater();
        }
    }
    updateInteractive();
    emit closed(key);
}

void PopoverHost::reposition()
{
    if (Popover* p = current())
        p->place(anchorRect(), rect());
}

void PopoverHost::mousePressEvent(QMouseEvent* event)
{
    // A press outside every popover dismisses them (and is consumed).
    closeAll();
    event->accept();
}

void PopoverHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    reposition();
}

bool PopoverHost::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    else if (watched == m_anchor && (event->type() == QEvent::Move || event->type() == QEvent::Resize))
        reposition();
    return QWidget::eventFilter(watched, event);
}

} // namespace cb
