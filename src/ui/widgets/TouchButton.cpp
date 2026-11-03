#include "ui/widgets/TouchButton.h"

#include "ui/UiContext.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace cb {

TouchButton::TouchButton(const UiContext& ui, const QString& icon, const QString& text, Style style, QWidget* parent)
    : QAbstractButton(parent)
    , m_ui(ui)
    , m_icon(icon)
    , m_style(style)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    if (!text.isEmpty())
        setToolTip(text);
}

void TouchButton::setIconName(const QString& name)
{
    m_icon = name;
    update();
}

void TouchButton::setStyle(Style style)
{
    m_style = style;
    updateGeometry();
    update();
}

void TouchButton::setIndicatorColor(const QColor& color)
{
    m_indicator = color;
    update();
}

void TouchButton::setHasOptions(bool on)
{
    m_hasOptions = on;
    update();
}

void TouchButton::setPrimary(bool on)
{
    m_primary = on;
    update();
}

void TouchButton::setDanger(bool on)
{
    m_danger = on;
    update();
}

QSize TouchButton::sizeHint() const
{
    const Theme& t = m_ui.theme;
    const QFontMetrics fm(t.font(t.metric(ThemeMetric::SmallFontSize)));
    switch (m_style) {
    case Style::Ribbon: {
        const int h = t.scaledInt(ThemeMetric::RibbonHeight) - t.dpi(12);
        const int w = std::max(t.scaledInt(ThemeMetric::RibbonButtonWidth), fm.horizontalAdvance(text()) + t.dpi(20));
        return QSize(w, h);
    }
    case Style::Tile: {
        const int s = t.dpi(96);
        return QSize(std::max(s, fm.horizontalAdvance(text()) + t.dpi(16)), t.dpi(88));
    }
    case Style::Row: {
        const QFontMetrics big(t.font());
        return QSize(t.dpi(56) + big.horizontalAdvance(text()) + t.dpi(24), t.scaledInt(ThemeMetric::TouchTarget));
    }
    case Style::Icon: {
        const int s = t.scaledInt(ThemeMetric::TouchTarget);
        return QSize(s, s);
    }
    case Style::Pill: {
        const QFontMetrics big(t.font());
        const int iconW = m_icon.isEmpty() ? 0 : t.dpi(30);
        return QSize(big.horizontalAdvance(text()) + iconW + t.dpi(36), t.dpi(48));
    }
    }
    return QSize(64, 64);
}

void TouchButton::enterEvent(QEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void TouchButton::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void TouchButton::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(t.dp(2), t.dp(2), -t.dp(2), -t.dp(2));

    QColor bg = t.color(ThemeColor::Button);
    QColor fg = isEnabled() ? t.color(ThemeColor::Text) : t.color(ThemeColor::TextDisabled);
    if (m_primary && isEnabled()) {
        bg = t.color(ThemeColor::Accent);
        fg = t.color(ThemeColor::AccentText);
    }
    if (m_danger && isEnabled())
        fg = t.color(ThemeColor::Danger);
    if (isChecked()) {
        bg = t.color(ThemeColor::ButtonChecked);
        fg = t.color(ThemeColor::Accent);
    }
    if (isEnabled()) {
        if (isDown())
            bg = m_primary ? bg.darker(115) : t.color(ThemeColor::ButtonPressed);
        else if (m_hover && !isChecked())
            bg = m_primary ? bg.lighter(110) : t.color(ThemeColor::ButtonHover);
    }
    if (m_style == Style::Tile && bg.alpha() == 0)
        bg = t.color(ThemeColor::ButtonHover).darker(115);
    if (m_style == Style::Pill && bg.alpha() == 0 && !m_primary)
        bg = t.color(ThemeColor::ButtonHover);

    const qreal radius = m_style == Style::Icon ? r.height() / 2
        : (m_style == Style::Pill ? r.height() / 2 : t.scaled(ThemeMetric::CornerRadius));
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);

    const qreal dpr = devicePixelRatioF();
    const QFont smallFont = t.font(t.metric(ThemeMetric::SmallFontSize), isChecked());
    const QFont normalFont = t.font(-1, m_primary);

    auto drawIcon = [&](const QPointF& center, qreal size) {
        if (m_icon.isEmpty())
            return;
        const QPixmap pm = m_ui.icons.pixmap(m_icon, size, fg, dpr);
        p.drawPixmap(QPointF(center.x() - size / 2, center.y() - size / 2), pm);
    };

    switch (m_style) {
    case Style::Ribbon:
    case Style::Tile: {
        const qreal iconSize = m_style == Style::Ribbon ? t.scaled(ThemeMetric::RibbonIconSize)
                                                         : t.scaled(ThemeMetric::IconSize) * 1.15;
        const bool hasText = !text().isEmpty();
        const qreal textH = hasText ? QFontMetricsF(smallFont).height() : 0;
        const qreal gap = hasText ? t.dp(5) : 0;
        const qreal total = iconSize + gap + textH;
        const qreal top = r.center().y() - total / 2;
        drawIcon(QPointF(r.center().x(), top + iconSize / 2), iconSize);
        if (hasText) {
            p.setFont(smallFont);
            p.setPen(fg);
            p.drawText(QRectF(r.left(), top + iconSize + gap, r.width(), textH), Qt::AlignCenter, text());
        }
        if (m_indicator.isValid()) {
            const qreal d = t.dp(9);
            const QPointF c(r.center().x() + iconSize * 0.62, top + iconSize * 0.18);
            p.setPen(QPen(t.color(ThemeColor::Ribbon), t.dp(2)));
            p.setBrush(m_indicator);
            p.drawEllipse(c, d / 2 + t.dp(1), d / 2 + t.dp(1));
        }
        if (m_hasOptions && isChecked()) {
            // Caret on top edge: "tap again for options".
            const qreal w = t.dp(10);
            QPainterPath caret;
            const QPointF c(r.center().x(), r.top() + t.dp(5));
            caret.moveTo(c + QPointF(-w / 2, w / 4));
            caret.lineTo(c + QPointF(0, -w / 4));
            caret.lineTo(c + QPointF(w / 2, w / 4));
            p.setPen(QPen(fg, t.dp(2), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(caret);
        }
        break;
    }
    case Style::Row: {
        const qreal iconSize = t.scaled(ThemeMetric::IconSize);
        drawIcon(QPointF(r.left() + t.dp(28), r.center().y()), iconSize);
        p.setFont(normalFont);
        p.setPen(fg);
        p.drawText(r.adjusted(t.dp(56), 0, -t.dp(8), 0), Qt::AlignVCenter | Qt::AlignLeft, text());
        break;
    }
    case Style::Icon: {
        drawIcon(r.center(), t.scaled(ThemeMetric::IconSize));
        if (m_indicator.isValid()) {
            p.setPen(QPen(t.color(ThemeColor::Popover), t.dp(2)));
            p.setBrush(m_indicator);
            p.drawEllipse(r.center(), r.width() * 0.3, r.width() * 0.3);
        }
        break;
    }
    case Style::Pill: {
        p.setFont(normalFont);
        p.setPen(fg);
        if (m_icon.isEmpty()) {
            p.drawText(r, Qt::AlignCenter, text());
        } else {
            const qreal iconSize = t.scaled(ThemeMetric::IconSize) * 0.85;
            const qreal textW = QFontMetricsF(normalFont).horizontalAdvance(text());
            const qreal total = iconSize + t.dp(8) + textW;
            const qreal left = r.center().x() - total / 2;
            drawIcon(QPointF(left + iconSize / 2, r.center().y()), iconSize);
            p.drawText(QRectF(left + iconSize + t.dp(8), r.top(), textW + t.dp(4), r.height()),
                       Qt::AlignVCenter | Qt::AlignLeft, text());
        }
        break;
    }
    }
}

} // namespace cb
