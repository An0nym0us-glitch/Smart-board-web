#pragma once

#include "document/DocumentObject.h"

#include <QStringList>

namespace cb {

/// Editable table (value tables for functions, data for lessons). The first row can be a header.
class TableObject final : public DocumentObject
{
public:
    TableObject();
    static std::unique_ptr<TableObject> create(int rows, int columns, const QPointF& center);

    int rowCount() const { return m_rows; }
    int columnCount() const { return m_cols; }
    QString cell(int row, int column) const;
    void setCell(int row, int column, const QString& text);
    void setDimensions(int rows, int columns);
    bool hasHeader() const { return m_header; }
    void setHeader(bool on) { m_header = on; }
    qreal fontSize() const { return m_fontSize; }

    /// Local rect of a cell.
    QRectF cellRect(int row, int column) const;
    /// Cell under a local point; returns false outside the table.
    bool cellAt(const QPointF& local, int* row, int* column) const;

    QRectF localBounds() const override;
    qreal outlineMargin() const override { return 2.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_color; }
    bool isEditable() const override { return true; }
    std::unique_ptr<DocumentObject> clone() const override;

protected:
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    TableObject(const TableObject&) = default;

    int m_rows = 3;
    int m_cols = 3;
    QStringList m_cells;
    qreal m_cellWidth = 150;
    qreal m_cellHeight = 56;
    qreal m_fontSize = 26;
    bool m_header = true;
    QColor m_color = QColor(235, 240, 235);
};

} // namespace cb
