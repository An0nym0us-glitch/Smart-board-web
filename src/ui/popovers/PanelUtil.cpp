#include "ui/popovers/PanelUtil.h"

#include "ui/UiContext.h"

#include <QHBoxLayout>

namespace cb::panel {

QVBoxLayout* makeLayout(const UiContext& ui, QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ui.theme.dpi(10));
    return layout;
}

QLabel* section(const UiContext& ui, const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    QFont f = ui.theme.font(ui.theme.metric(ThemeMetric::SmallFontSize), true);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    label->setFont(f);
    label->setStyleSheet(QStringLiteral("color: %1; padding-top: %2px;")
                             .arg(ui.theme.color(ThemeColor::TextMuted).name())
                             .arg(ui.theme.dpi(4)));
    return label;
}

QLabel* hint(const UiContext& ui, const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setFont(ui.theme.font(ui.theme.metric(ThemeMetric::SmallFontSize)));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(ui.theme.color(ThemeColor::TextMuted).name()));
    return label;
}

QWidget* tileGrid(const UiContext& ui, int columns, const QVector<Tile>& tiles, QWidget* parent,
                  QVector<TouchButton*>* buttons)
{
    auto* w = new QWidget(parent);
    auto* grid = new QGridLayout(w);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(ui.theme.dpi(8));
    for (int i = 0; i < tiles.size(); ++i) {
        const Tile& t = tiles[i];
        auto* b = new TouchButton(ui, t.icon, t.label, TouchButton::Style::Tile, w);
        b->setCheckable(t.checked);
        b->setChecked(t.checked);
        b->setEnabled(t.enabled);
        const auto action = t.action;
        QObject::connect(b, &QAbstractButton::clicked, w, [b, action, checked = t.checked]() {
            if (b->isCheckable())
                b->setChecked(checked);
            if (action)
                action();
        });
        grid->addWidget(b, i / columns, i % columns);
        if (buttons)
            buttons->push_back(b);
    }
    return w;
}

TouchButton* pill(const UiContext& ui, const QString& icon, const QString& text, QWidget* parent,
                  std::function<void()> action, bool primary)
{
    auto* b = new TouchButton(ui, icon, text, TouchButton::Style::Pill, parent);
    b->setPrimary(primary);
    if (action)
        QObject::connect(b, &QAbstractButton::clicked, parent, std::move(action));
    return b;
}

QWidget* choices(const UiContext& ui, const QStringList& labels, int current, QWidget* parent,
                 std::function<void(int)> onPick, int columns, QVector<TouchButton*>* buttons)
{
    auto* w = new QWidget(parent);
    auto* grid = new QGridLayout(w);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(ui.theme.dpi(6));
    auto* list = new QVector<TouchButton*>();
    QObject::connect(w, &QObject::destroyed, [list]() { delete list; });
    for (int i = 0; i < labels.size(); ++i) {
        auto* b = new TouchButton(ui, QString(), labels[i], TouchButton::Style::Pill, w);
        b->setCheckable(true);
        b->setChecked(i == current);
        list->push_back(b);
        QObject::connect(b, &QAbstractButton::clicked, w, [list, i, onPick]() {
            for (int j = 0; j < list->size(); ++j)
                (*list)[j]->setChecked(j == i);
            if (onPick)
                onPick(i);
        });
        grid->addWidget(b, i / columns, i % columns);
        if (buttons)
            buttons->push_back(b);
    }
    return w;
}

QWidget* row(const UiContext& ui, const QVector<QWidget*>& widgets, QWidget* parent, bool stretchEnd)
{
    auto* w = new QWidget(parent);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ui.theme.dpi(8));
    for (QWidget* child : widgets) {
        child->setParent(w);
        layout->addWidget(child);
    }
    if (stretchEnd)
        layout->addStretch(1);
    return w;
}

} // namespace cb::panel
