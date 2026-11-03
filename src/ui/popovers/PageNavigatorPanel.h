#pragma once

#include <QElapsedTimer>
#include <QWidget>

namespace cb {

struct AppServices;

/// Grid of page thumbnails: tap to open a page, drag to reorder.
class PageGrid : public QWidget
{
    Q_OBJECT
public:
    PageGrid(const AppServices& services, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    int columns() const { return m_columns; }

signals:
    void pageActivated(int index);
    void pageMoved(int from, int to);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF tileRect(int index) const;
    int indexAt(const QPointF& pos) const;
    int dropIndex(const QPointF& pos) const;
    QSizeF tileSize() const;

    const AppServices& m_s;
    int m_columns = 4;
    int m_pressIndex = -1;
    QPointF m_pressPos;
    QPointF m_dragPos;
    bool m_dragging = false;
};

/// Pages popover: navigator grid plus page actions (new, duplicate, delete, move, rename, background).
class PageNavigatorPanel : public QWidget
{
    Q_OBJECT
public:
    PageNavigatorPanel(const AppServices& services, QWidget* parent = nullptr);

private:
    const AppServices& m_s;
};

} // namespace cb
