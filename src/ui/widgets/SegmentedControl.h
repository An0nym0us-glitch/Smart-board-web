#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

namespace cb {

struct UiContext;

/// Row of mutually exclusive options (icon + label), touch sized.
class SegmentedControl : public QWidget
{
    Q_OBJECT
public:
    struct Option
    {
        QString icon;
        QString label;
    };

    SegmentedControl(const UiContext& ui, const QVector<Option>& options, QWidget* parent = nullptr);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    QSize sizeHint() const override;
    /// Area of one option. Options get widths in proportion to their labels, so a long label
    /// (wrapped on two lines) does not squeeze its neighbours.
    QRectF segmentRect(int i) const;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    qreal naturalWidth(int i) const;

    const UiContext& m_ui;
    QVector<Option> m_options;
    int m_current = 0;
};

} // namespace cb
