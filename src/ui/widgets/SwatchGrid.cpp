#include "ui/widgets/SwatchGrid.h"

#include "ui/UiContext.h"

#include <QMouseEvent>
#include <QPainter>

namespace cb {

SwatchGrid::SwatchGrid(const UiContext& ui, int columns, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
    , m_columns(std::max(1, columns))
{
    setCursor(Qt::PointingHandCursor);
}

void SwatchGrid::setColors(const QVector<QColor>& colors)
{
    m_colors = colors;
    updateGeometry();
    update();
}

void SwatchGrid::setCurrent(const QColor& color)
{
    m_current = color;
    update();
}

qreal SwatchGrid::cellSize() const
{
    return m_ui.theme.dp(50);
}

QSize SwatchGrid::sizeHint() const
{
    const int rows = (m_colors.size() + m_columns - 1) / m_columns;
    const qreal c = cellSize();
    return QSize(qRound(c * m_columns), qRound(c * std::max(1, rows)));
}

QRectF SwatchGrid::cellRect(int index) const
{
    const qreal c = cellSize();
    const int row = index / m_columns;
    const int col = index % m_columns;
    return QRectF(col * c, row * c, c, c);
}

void SwatchGrid::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < m_colors.size(); ++i) {
        const QRectF cell = cellRect(i);
        const QColor& c = m_colors[i];
        const bool selected = c.rgb() == m_current.rgb();
        const qreal r = cell.width() * 0.34;
        if (selected) {
            p.setPen(QPen(t.color(ThemeColor::Accent), t.dp(3)));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(cell.center(), r + t.dp(5), r + t.dp(5));
        }
        p.setPen(QPen(QColor(255, 255, 255, 60), t.dp(1)));
        p.setBrush(c);
        p.drawEllipse(cell.center(), r, r);
    }
}

void SwatchGrid::mouseReleaseEvent(QMouseEvent* e)
{
    for (int i = 0; i < m_colors.size(); ++i) {
        if (cellRect(i).contains(e->localPos())) {
            m_current = m_colors[i];
            update();
            emit colorPicked(m_current);
            return;
        }
    }
}

} // namespace cb
