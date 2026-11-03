#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

namespace cb {

struct UiContext;

/// Grid of large colour swatches.
class SwatchGrid : public QWidget
{
    Q_OBJECT
public:
    SwatchGrid(const UiContext& ui, int columns, QWidget* parent = nullptr);

    void setColors(const QVector<QColor>& colors);
    void setCurrent(const QColor& color);
    QColor current() const { return m_current; }

    QSize sizeHint() const override;

signals:
    void colorPicked(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF cellRect(int index) const;
    qreal cellSize() const;

    const UiContext& m_ui;
    int m_columns;
    QVector<QColor> m_colors;
    QColor m_current;
};

} // namespace cb
