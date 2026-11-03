#pragma once

#include <QWidget>

namespace cb {

struct UiContext;

/// Label with a large on/off switch; the whole row is the touch target.
class ToggleRow : public QWidget
{
    Q_OBJECT
public:
    ToggleRow(const UiContext& ui, const QString& label, QWidget* parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool on);
    void setDescription(const QString& text);

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    const UiContext& m_ui;
    QString m_label;
    QString m_description;
    bool m_checked = false;
};

} // namespace cb
