#pragma once

#include <QWidget>

#include <functional>

namespace cb {

struct UiContext;

/// Large touch slider with an optional title and live value read-out.
class TouchSlider : public QWidget
{
    Q_OBJECT
public:
    TouchSlider(const UiContext& ui, const QString& title, QWidget* parent = nullptr);

    void setRange(double min, double max);
    void setStep(double step) { m_step = step; }
    void setValue(double v);
    double value() const { return m_value; }
    /// Formats the value read-out (default: rounded number).
    void setFormatter(std::function<QString(double)> f) { m_formatter = std::move(f); }
    /// Optional preview painter drawn at the left (e.g. pen thickness dot).
    void setPreview(std::function<void(QPainter&, const QRectF&, double)> f) { m_preview = std::move(f); }

    QSize sizeHint() const override;

signals:
    void valueChanged(double value);
    void sliderReleased();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF trackRect() const;
    void setFromX(qreal x);

    const UiContext& m_ui;
    QString m_title;
    double m_min = 0.0;
    double m_max = 1.0;
    double m_step = 0.0;
    double m_value = 0.0;
    bool m_dragging = false;
    std::function<QString(double)> m_formatter;
    std::function<void(QPainter&, const QRectF&, double)> m_preview;
};

} // namespace cb
