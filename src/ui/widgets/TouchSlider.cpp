#include "ui/widgets/TouchSlider.h"

#include "core/Geometry.h"
#include "ui/UiContext.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cb {

TouchSlider::TouchSlider(const UiContext& ui, const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
    , m_title(title)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TouchSlider::setRange(double min, double max)
{
    m_min = min;
    m_max = std::max(max, min + 1e-9);
    setValue(m_value);
}

void TouchSlider::setValue(double v)
{
    v = std::clamp(v, m_min, m_max);
    if (m_step > 0)
        v = m_min + std::round((v - m_min) / m_step) * m_step;
    if (v == m_value)
        return;
    m_value = v;
    update();
}

QSize TouchSlider::sizeHint() const
{
    const Theme& t = m_ui.theme;
    const int h = (m_title.isEmpty() ? 0 : t.dpi(24)) + t.dpi(48);
    return QSize(t.dpi(300), h);
}

QRectF TouchSlider::trackRect() const
{
    const Theme& t = m_ui.theme;
    const qreal top = m_title.isEmpty() ? 0 : t.dp(24);
    const qreal left = m_preview ? t.dp(52) : t.dp(16);
    return QRectF(left, top + t.dp(22), width() - left - t.dp(16), t.dp(6));
}

void TouchSlider::setFromX(qreal x)
{
    const QRectF tr = trackRect();
    const double f = std::clamp((x - tr.left()) / tr.width(), 0.0, 1.0);
    const double old = m_value;
    setValue(m_min + f * (m_max - m_min));
    if (m_value != old)
        emit valueChanged(m_value);
}

void TouchSlider::mousePressEvent(QMouseEvent* e)
{
    m_dragging = true;
    setFromX(e->localPos().x());
}

void TouchSlider::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging)
        setFromX(e->localPos().x());
}

void TouchSlider::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_dragging)
        setFromX(e->localPos().x());
    m_dragging = false;
    emit sliderReleased();
}

void TouchSlider::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!m_title.isEmpty()) {
        p.setFont(t.font(t.metric(ThemeMetric::SmallFontSize), true));
        p.setPen(t.color(ThemeColor::TextMuted));
        p.drawText(QRectF(0, 0, width(), t.dp(22)), Qt::AlignLeft | Qt::AlignVCenter, m_title);
        const QString v = m_formatter ? m_formatter(m_value) : geom::formatNumber(m_value, 1);
        p.setPen(t.color(ThemeColor::Text));
        p.drawText(QRectF(0, 0, width(), t.dp(22)), Qt::AlignRight | Qt::AlignVCenter, v);
    }
    const QRectF tr = trackRect();
    if (m_preview) {
        const QRectF box(0, tr.center().y() - t.dp(22), t.dp(44), t.dp(44));
        m_preview(p, box, m_value);
    }
    const double f = (m_value - m_min) / (m_max - m_min);
    p.setPen(Qt::NoPen);
    p.setBrush(t.color(ThemeColor::ButtonPressed));
    p.drawRoundedRect(tr, tr.height() / 2, tr.height() / 2);
    QRectF filled = tr;
    filled.setWidth(tr.width() * f);
    p.setBrush(t.color(ThemeColor::Accent));
    p.drawRoundedRect(filled, tr.height() / 2, tr.height() / 2);
    const QPointF thumb(tr.left() + tr.width() * f, tr.center().y());
    const qreal r = t.dp(13);
    p.setBrush(t.color(ThemeColor::Handle));
    p.setPen(QPen(t.color(ThemeColor::Accent), t.dp(3)));
    p.drawEllipse(thumb, r, r);
}

} // namespace cb
