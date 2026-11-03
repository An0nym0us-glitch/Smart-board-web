#include "ui/widgets/SegmentedControl.h"

#include "ui/UiContext.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace cb {

SegmentedControl::SegmentedControl(const UiContext& ui, const QVector<Option>& options, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
    , m_options(options)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_options.size() || index == m_current)
        return;
    m_current = index;
    update();
}

qreal SegmentedControl::naturalWidth(int i) const
{
    const Theme& t = m_ui.theme;
    // Measured bold (the selected look) so the label never grows out of its segment.
    const QFontMetricsF fm(t.font(t.metric(ThemeMetric::SmallFontSize), true));
    const Option& o = m_options[i];
    // Labels with an icon may wrap, so the longest word decides; text-only labels stay on one line.
    qreal text = 0.0;
    if (o.icon.isEmpty()) {
        text = fm.horizontalAdvance(o.label);
    } else {
        for (const QString& word : o.label.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            text = std::max(text, fm.horizontalAdvance(word));
    }
    return std::max<qreal>(t.dpi(64), text + t.dpi(20));
}

QSize SegmentedControl::sizeHint() const
{
    const Theme& t = m_ui.theme;
    qreal w = 0;
    bool anyIcon = false;
    for (int i = 0; i < m_options.size(); ++i) {
        w += naturalWidth(i);
        anyIcon = anyIcon || !m_options[i].icon.isEmpty();
    }
    return QSize(qCeil(w), anyIcon ? t.dpi(72) : t.dpi(48));
}

QRectF SegmentedControl::segmentRect(int i) const
{
    qreal total = 0.0, before = 0.0;
    for (int k = 0; k < m_options.size(); ++k) {
        const qreal w = naturalWidth(k);
        total += w;
        if (k < i)
            before += w;
    }
    if (i < 0 || i >= m_options.size() || total <= 0.0)
        return QRectF();
    const qreal scale = width() / total;
    return QRectF(before * scale, 0, naturalWidth(i) * scale, height());
}

void SegmentedControl::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = t.scaled(ThemeMetric::CornerRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(t.color(ThemeColor::Input));
    p.drawRoundedRect(QRectF(rect()), radius, radius);
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < m_options.size(); ++i) {
        const QRectF r = segmentRect(i).adjusted(t.dp(3), t.dp(3), -t.dp(3), -t.dp(3));
        const bool sel = i == m_current;
        if (sel) {
            p.setBrush(t.color(ThemeColor::ButtonChecked));
            p.setPen(QPen(t.color(ThemeColor::Accent), t.dp(1.5)));
            p.drawRoundedRect(r, radius - t.dp(3), radius - t.dp(3));
        }
        const QColor fg = sel ? t.color(ThemeColor::Accent) : t.color(ThemeColor::Text);
        const Option& o = m_options[i];
        const QFont f = t.font(t.metric(ThemeMetric::SmallFontSize), sel);
        p.setFont(f);
        p.setPen(fg);
        if (o.icon.isEmpty()) {
            p.drawText(r, Qt::AlignCenter, o.label);
        } else {
            const qreal iconSize = t.scaled(ThemeMetric::IconSize);
            // A label that does not fit the segment ("Magic Highlighter") wraps onto two lines.
            const QFontMetricsF fm(f);
            const QRectF textBox(r.left() + t.dp(2), 0, r.width() - t.dp(4), r.height());
            const int flags = Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap;
            const qreal textH = o.label.isEmpty() ? 0 : fm.boundingRect(textBox, flags, o.label).height();
            const qreal total = iconSize + (textH > 0 ? t.dp(4) + textH : 0);
            const qreal top = std::max(r.top(), r.center().y() - total / 2);
            p.drawPixmap(QPointF(r.center().x() - iconSize / 2, top), m_ui.icons.pixmap(o.icon, iconSize, fg, dpr));
            if (textH > 0)
                p.drawText(QRectF(textBox.left(), top + iconSize + t.dp(4), textBox.width(), textH), flags, o.label);
        }
    }
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent* e)
{
    for (int i = 0; i < m_options.size(); ++i) {
        if (segmentRect(i).contains(e->localPos())) {
            if (i != m_current) {
                m_current = i;
                update();
                emit currentChanged(i);
            }
            return;
        }
    }
}

} // namespace cb
