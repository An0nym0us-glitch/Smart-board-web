#include "ui/Theme.h"

#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace cb {

namespace {
struct ColorKey
{
    ThemeColor role;
    const char* key;
    const char* value;
};

// Built-in "Blackboard Dark" defaults, overridden by the theme file.
const ColorKey kColors[] = {
    {ThemeColor::Window, "window", "#141816"},
    {ThemeColor::Ribbon, "ribbon", "#191d1b"},
    {ThemeColor::RibbonBorder, "ribbonBorder", "#2a302d"},
    {ThemeColor::Button, "button", "#00000000"},
    {ThemeColor::ButtonHover, "buttonHover", "#262c29"},
    {ThemeColor::ButtonPressed, "buttonPressed", "#303833"},
    {ThemeColor::ButtonChecked, "buttonChecked", "#2c3a33"},
    {ThemeColor::Accent, "accent", "#5fd3a5"},
    {ThemeColor::AccentText, "accentText", "#0d1a14"},
    {ThemeColor::Text, "text", "#e9eeeb"},
    {ThemeColor::TextMuted, "textMuted", "#9aa7a0"},
    {ThemeColor::TextDisabled, "textDisabled", "#59625d"},
    {ThemeColor::Popover, "popover", "#202624"},
    {ThemeColor::PopoverBorder, "popoverBorder", "#343d39"},
    {ThemeColor::PopoverHeader, "popoverHeader", "#c9d3ce"},
    {ThemeColor::Shadow, "shadow", "#000000"},
    {ThemeColor::Separator, "separator", "#2f3633"},
    {ThemeColor::Selection, "selection", "#4fb3ff"},
    {ThemeColor::SelectionFill, "selectionFill", "#1a4fb3ff"},
    {ThemeColor::Handle, "handle", "#ffffff"},
    {ThemeColor::Danger, "danger", "#ef6a64"},
    {ThemeColor::Success, "success", "#6fcf97"},
    {ThemeColor::Input, "input", "#151a18"},
    {ThemeColor::InputBorder, "inputBorder", "#3a4440"},
    {ThemeColor::Scrim, "scrim", "#99000000"},
};

struct MetricKey
{
    ThemeMetric role;
    const char* key;
    qreal value;
};

const MetricKey kMetrics[] = {
    {ThemeMetric::RibbonHeight, "ribbonHeight", 78},
    {ThemeMetric::RibbonButtonWidth, "ribbonButtonWidth", 78},
    {ThemeMetric::IconSize, "iconSize", 26},
    {ThemeMetric::RibbonIconSize, "ribbonIconSize", 28},
    {ThemeMetric::CornerRadius, "cornerRadius", 12},
    {ThemeMetric::PopoverRadius, "popoverRadius", 18},
    {ThemeMetric::PopoverPadding, "popoverPadding", 16},
    {ThemeMetric::PopoverNotch, "popoverNotch", 12},
    {ThemeMetric::Spacing, "spacing", 8},
    {ThemeMetric::TouchTarget, "touchTarget", 56},
    {ThemeMetric::FontSize, "fontSize", 15},
    {ThemeMetric::SmallFontSize, "smallFontSize", 12.5},
    {ThemeMetric::TitleFontSize, "titleFontSize", 17},
    {ThemeMetric::HandleSize, "handleSize", 14},
    {ThemeMetric::HandleHitSize, "handleHitSize", 44},
};
} // namespace

Theme::Theme()
{
    for (const auto& c : kColors)
        m_colors.insert(static_cast<int>(c.role), QColor(QString::fromLatin1(c.value)));
    for (const auto& m : kMetrics)
        m_metrics.insert(static_cast<int>(m.role), m.value);
#ifdef Q_OS_WIN
    m_fontFamily = QStringLiteral("Segoe UI");
#else
    m_fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
}

bool Theme::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    m_name = root.value(QStringLiteral("name")).toString(m_name);
    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    for (const auto& c : kColors) {
        const QString v = colors.value(QString::fromLatin1(c.key)).toString();
        const QColor col(v);
        if (col.isValid())
            m_colors.insert(static_cast<int>(c.role), col);
    }
    const QJsonObject metrics = root.value(QStringLiteral("metrics")).toObject();
    for (const auto& m : kMetrics) {
        const QJsonValue v = metrics.value(QString::fromLatin1(m.key));
        if (v.isDouble())
            m_metrics.insert(static_cast<int>(m.role), v.toDouble());
    }
    const QJsonObject fonts = root.value(QStringLiteral("fonts")).toObject();
    const QJsonArray families = fonts.value(QStringLiteral("families")).toArray();
    const QStringList available = QFontDatabase().families();
    for (const QJsonValue& fam : families) {
        if (available.contains(fam.toString(), Qt::CaseInsensitive)) {
            m_fontFamily = fam.toString();
            break;
        }
    }
    return true;
}

QColor Theme::color(ThemeColor role) const
{
    return m_colors.value(static_cast<int>(role), QColor(Qt::magenta));
}

qreal Theme::metric(ThemeMetric role) const
{
    return m_metrics.value(static_cast<int>(role), 0.0);
}

void Theme::setUiScale(qreal scale)
{
    // 0.45 leaves room for a slim ribbon (Settings → Ribbon size) on a small interface size.
    m_scale = std::clamp(scale, 0.45, 3.0);
}

QFont Theme::font(qreal pixelSizeDp, bool bold) const
{
    QFont f(m_fontFamily);
    const qreal size = pixelSizeDp > 0 ? pixelSizeDp : metric(ThemeMetric::FontSize);
    f.setPixelSize(std::max(8, qRound(size * m_scale)));
    f.setBold(bold);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QString Theme::styleSheet() const
{
    const auto c = [this](ThemeColor r) { return color(r).name(QColor::HexArgb); };
    const int radius = qRound(dp(10));
    const int pad = qRound(dp(10));
    const int font = qRound(dp(metric(ThemeMetric::FontSize)));
    const int scroll = qRound(dp(10));
    return QStringLiteral(
               "QWidget { color: %1; font-family: '%2'; font-size: %3px; }"
               "QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox {"
               "  background: %4; border: 1px solid %5; border-radius: %6px; padding: %7px;"
               "  selection-background-color: %8; selection-color: %9; }"
               "QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid %8; }"
               "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"
               "QScrollBar:vertical { background: transparent; width: %10px; margin: 2px; }"
               "QScrollBar::handle:vertical { background: %11; border-radius: %12px; min-height: %13px; }"
               "QScrollBar:horizontal { background: transparent; height: %10px; margin: 2px; }"
               "QScrollBar::handle:horizontal { background: %11; border-radius: %12px; min-width: %13px; }"
               "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
               "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
               "QToolTip { background: %14; color: %1; border: 1px solid %5; padding: 6px; }")
        .arg(c(ThemeColor::Text), m_fontFamily)
        .arg(font)
        .arg(c(ThemeColor::Input), c(ThemeColor::InputBorder))
        .arg(radius)
        .arg(pad)
        .arg(c(ThemeColor::Accent), c(ThemeColor::AccentText))
        .arg(scroll)
        .arg(c(ThemeColor::ButtonPressed))
        .arg(scroll / 2)
        .arg(qRound(dp(40)))
        .arg(c(ThemeColor::Popover));
}

} // namespace cb
