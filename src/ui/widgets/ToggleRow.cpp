#include "ui/widgets/ToggleRow.h"

#include "ui/UiContext.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace cb {

ToggleRow::ToggleRow(const UiContext& ui, const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
    , m_label(label)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ToggleRow::setChecked(bool on)
{
    if (on == m_checked)
        return;
    m_checked = on;
    update();
}

void ToggleRow::setDescription(const QString& text)
{
    m_description = text;
    updateGeometry();
    update();
}

QSize ToggleRow::sizeHint() const
{
    return QSize(m_ui.theme.dpi(300), heightForWidth(m_ui.theme.dpi(300)));
}

int ToggleRow::heightForWidth(int width) const
{
    const Theme& t = m_ui.theme;
    if (m_description.isEmpty())
        return t.dpi(52);
    const int textWidth = std::max(t.dpi(80), width - t.dpi(64));
    const QFontMetrics fm(t.font(t.metric(ThemeMetric::SmallFontSize)));
    const int descHeight = fm.boundingRect(QRect(0, 0, textWidth, 1000), Qt::TextWordWrap, m_description).height();
    return t.dpi(40) + descHeight + t.dpi(8);
}

void ToggleRow::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal sw = t.dp(52);
    const qreal sh = t.dp(30);
    const QRectF track(width() - sw - t.dp(4), m_description.isEmpty() ? (height() - sh) / 2 : t.dp(10), sw, sh);
    const QRectF textRect(0, 0, track.left() - t.dp(8), height());
    p.setPen(t.color(ThemeColor::Text));
    p.setFont(t.font());
    if (m_description.isEmpty()) {
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_label);
    } else {
        p.drawText(QRectF(0, t.dp(8), textRect.width(), t.dp(24)), Qt::AlignLeft | Qt::AlignVCenter, m_label);
        p.setFont(t.font(t.metric(ThemeMetric::SmallFontSize)));
        p.setPen(t.color(ThemeColor::TextMuted));
        p.drawText(QRectF(0, t.dp(32), textRect.width(), height() - t.dp(32)), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   m_description);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(m_checked ? t.color(ThemeColor::Accent) : t.color(ThemeColor::ButtonPressed));
    p.drawRoundedRect(track, sh / 2, sh / 2);
    const qreal r = sh / 2 - t.dp(3);
    const QPointF c(m_checked ? track.right() - sh / 2 : track.left() + sh / 2, track.center().y());
    p.setBrush(t.color(ThemeColor::Handle));
    p.drawEllipse(c, r, r);
}

void ToggleRow::mouseReleaseEvent(QMouseEvent* e)
{
    if (!rect().contains(e->pos()))
        return;
    m_checked = !m_checked;
    update();
    emit toggled(m_checked);
}

} // namespace cb
