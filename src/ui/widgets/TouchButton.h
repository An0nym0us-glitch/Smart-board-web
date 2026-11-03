#pragma once

#include <QAbstractButton>
#include <QColor>

namespace cb {

struct UiContext;

/// Large, touch friendly button painted from the theme.
class TouchButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum class Style {
        Ribbon,   ///< icon above label, fixed ribbon height
        Tile,     ///< square tile with icon above label (MORE grid)
        Row,      ///< icon + label left aligned (lists)
        Icon,     ///< round icon only
        Pill,     ///< compact rounded text button (actions)
    };

    TouchButton(const UiContext& ui, const QString& icon, const QString& text, Style style, QWidget* parent = nullptr);

    void setIconName(const QString& name);
    QString iconName() const { return m_icon; }
    void setStyle(Style style);

    /// Small colour dot (e.g. current pen colour) drawn next to the icon. Invalid colour hides it.
    void setIndicatorColor(const QColor& color);
    /// Shows a small caret meaning "tap again for options".
    void setHasOptions(bool on);
    /// Accent (primary action) colouring.
    void setPrimary(bool on);
    void setDanger(bool on);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    const UiContext& m_ui;
    QString m_icon;
    Style m_style;
    QColor m_indicator;
    bool m_hasOptions = false;
    bool m_primary = false;
    bool m_danger = false;
    bool m_hover = false;
};

} // namespace cb
