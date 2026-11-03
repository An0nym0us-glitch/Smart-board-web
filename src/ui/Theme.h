#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QString>

namespace cb {

/// Colour roles used throughout the interface. No widget hard-codes colours.
enum class ThemeColor {
    Window,
    Ribbon,
    RibbonBorder,
    Button,
    ButtonHover,
    ButtonPressed,
    ButtonChecked,
    Accent,
    AccentText,
    Text,
    TextMuted,
    TextDisabled,
    Popover,
    PopoverBorder,
    PopoverHeader,
    Shadow,
    Separator,
    Selection,
    SelectionFill,
    Handle,
    Danger,
    Success,
    Input,
    InputBorder,
    Scrim,
};

/// Size roles in device independent pixels (before UI scaling).
enum class ThemeMetric {
    RibbonHeight,
    RibbonButtonWidth,
    IconSize,
    RibbonIconSize,
    CornerRadius,
    PopoverRadius,
    PopoverPadding,
    PopoverNotch,
    Spacing,
    TouchTarget,
    FontSize,
    SmallFontSize,
    TitleFontSize,
    HandleSize,
    HandleHitSize,
};

/// Centralised, data driven theme (loaded from resources/themes/*.json).
/// All sizes pass through dp() so the whole UI scales for 1080p, 1440p and 4K classroom displays.
class Theme
{
public:
    Theme();

    /// Loads a theme file; missing keys keep built-in defaults. Returns false on parse errors.
    bool load(const QString& path);

    QString name() const { return m_name; }

    QColor color(ThemeColor role) const;
    qreal metric(ThemeMetric role) const;

    /// Scaled size helper.
    qreal dp(qreal v) const { return v * m_scale; }
    int dpi(qreal v) const { return qRound(v * m_scale); }
    qreal scaled(ThemeMetric role) const { return metric(role) * m_scale; }
    int scaledInt(ThemeMetric role) const { return qRound(metric(role) * m_scale); }

    qreal uiScale() const { return m_scale; }
    void setUiScale(qreal scale);

    QFont font(qreal pointSizeDp = -1, bool bold = false) const;
    QString fontFamily() const { return m_fontFamily; }

    /// Qt style sheet for standard widgets (line edits, scroll bars, tool tips) derived from roles.
    QString styleSheet() const;

private:
    QString m_name = QStringLiteral("Blackboard Dark");
    QHash<int, QColor> m_colors;
    QHash<int, qreal> m_metrics;
    QString m_fontFamily;
    qreal m_scale = 1.0;
};

} // namespace cb
