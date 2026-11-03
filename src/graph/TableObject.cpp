#include "graph/TableObject.h"

#include "core/JsonUtil.h"

#include <QFont>
#include <QJsonArray>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cb {

TableObject::TableObject()
    : DocumentObject(ObjectType::Table)
{
    setDimensions(3, 3);
}

std::unique_ptr<TableObject> TableObject::create(int rows, int columns, const QPointF& center)
{
    auto t = std::make_unique<TableObject>();
    t->setDimensions(rows, columns);
    t->setPosition(center);
    return t;
}

QString TableObject::cell(int row, int column) const
{
    if (row < 0 || row >= m_rows || column < 0 || column >= m_cols)
        return QString();
    return m_cells.value(row * m_cols + column);
}

void TableObject::setCell(int row, int column, const QString& text)
{
    if (row < 0 || row >= m_rows || column < 0 || column >= m_cols)
        return;
    m_cells[row * m_cols + column] = text;
}

void TableObject::setDimensions(int rows, int columns)
{
    rows = std::clamp(rows, 1, 60);
    columns = std::clamp(columns, 1, 20);
    QStringList cells;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c)
            cells << ((r < m_rows && c < m_cols) ? m_cells.value(r * m_cols + c) : QString());
    m_rows = rows;
    m_cols = columns;
    m_cells = cells;
    invalidateBounds();
}

QRectF TableObject::localBounds() const
{
    const qreal w = m_cols * m_cellWidth;
    const qreal h = m_rows * m_cellHeight;
    return QRectF(-w / 2, -h / 2, w, h);
}

QRectF TableObject::cellRect(int row, int column) const
{
    const QRectF b = localBounds();
    return QRectF(b.left() + column * m_cellWidth, b.top() + row * m_cellHeight, m_cellWidth, m_cellHeight);
}

bool TableObject::cellAt(const QPointF& local, int* row, int* column) const
{
    const QRectF b = localBounds();
    if (!b.contains(local))
        return false;
    const int c = std::clamp(static_cast<int>((local.x() - b.left()) / m_cellWidth), 0, m_cols - 1);
    const int r = std::clamp(static_cast<int>((local.y() - b.top()) / m_cellHeight), 0, m_rows - 1);
    if (row)
        *row = r;
    if (column)
        *column = c;
    return true;
}

void TableObject::paint(QPainter& p, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    const QRectF b = localBounds();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(10, 14, 12, 150));
    p.drawRect(b);
    if (m_header) {
        QColor h = m_color;
        h.setAlpha(45);
        p.setBrush(h);
        p.drawRect(QRectF(b.left(), b.top(), b.width(), m_cellHeight));
    }
    QPen grid(QColor(m_color.red(), m_color.green(), m_color.blue(), 150), 1.5);
    p.setPen(grid);
    p.setBrush(Qt::NoBrush);
    for (int r = 0; r <= m_rows; ++r) {
        const qreal y = b.top() + r * m_cellHeight;
        QPen pen = grid;
        if (m_header && r == 1)
            pen.setWidthF(3.0);
        p.setPen(pen);
        p.drawLine(QPointF(b.left(), y), QPointF(b.right(), y));
    }
    p.setPen(grid);
    for (int c = 0; c <= m_cols; ++c) {
        const qreal x = b.left() + c * m_cellWidth;
        p.drawLine(QPointF(x, b.top()), QPointF(x, b.bottom()));
    }
    QFont f = p.font();
    f.setPixelSize(std::max(6, static_cast<int>(std::lround(m_fontSize))));
    f.setHintingPreference(QFont::PreferNoHinting);
    for (int r = 0; r < m_rows; ++r) {
        f.setBold(m_header && r == 0);
        p.setFont(f);
        p.setPen(m_color);
        for (int c = 0; c < m_cols; ++c) {
            const QString text = m_cells.value(r * m_cols + c);
            if (!text.isEmpty())
                p.drawText(cellRect(r, c).adjusted(6, 2, -6, -2), Qt::AlignCenter | Qt::TextWordWrap, text);
        }
    }
    p.restore();
}

bool TableObject::setColor(const QColor& color)
{
    m_color = color;
    return true;
}

std::unique_ptr<DocumentObject> TableObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new TableObject(*this));
}

void TableObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    const qreal oldH = m_cellHeight;
    m_cellWidth = std::max(20.0, newSize.width() / m_cols);
    m_cellHeight = std::max(16.0, newSize.height() / m_rows);
    if (hint != ResizeHint::Horizontal && oldH > 0)
        m_fontSize = std::clamp(m_fontSize * m_cellHeight / oldH, 6.0, 400.0);
}

void TableObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("rows"), m_rows);
    obj.insert(QStringLiteral("cols"), m_cols);
    obj.insert(QStringLiteral("cells"), QJsonArray::fromStringList(m_cells));
    obj.insert(QStringLiteral("cellSize"), json::fromSize(QSizeF(m_cellWidth, m_cellHeight)));
    obj.insert(QStringLiteral("fontSize"), m_fontSize);
    obj.insert(QStringLiteral("header"), m_header);
    obj.insert(QStringLiteral("color"), json::fromColor(m_color));
}

bool TableObject::readProperties(const QJsonObject& obj)
{
    m_rows = std::clamp(obj.value(QStringLiteral("rows")).toInt(3), 1, 60);
    m_cols = std::clamp(obj.value(QStringLiteral("cols")).toInt(3), 1, 20);
    m_cells.clear();
    for (const QJsonValue& v : obj.value(QStringLiteral("cells")).toArray())
        m_cells << v.toString();
    while (m_cells.size() < m_rows * m_cols)
        m_cells << QString();
    const QSizeF cs = json::toSize(obj.value(QStringLiteral("cellSize")), QSizeF(150, 56));
    m_cellWidth = std::max(20.0, cs.width());
    m_cellHeight = std::max(16.0, cs.height());
    m_fontSize = std::clamp(obj.value(QStringLiteral("fontSize")).toDouble(26), 6.0, 400.0);
    m_header = obj.value(QStringLiteral("header")).toBool(true);
    m_color = json::toColor(obj.value(QStringLiteral("color")), m_color);
    return true;
}

} // namespace cb
