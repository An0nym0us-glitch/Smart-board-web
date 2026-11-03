#pragma once

#include "ui/widgets/TouchButton.h"

#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <functional>

namespace cb {

struct UiContext;

/// Small helpers so every popover panel uses the same typography and spacing.
namespace panel {

/// Vertical panel content with themed spacing.
QVBoxLayout* makeLayout(const UiContext& ui, QWidget* parent);

/// Muted small-caps section heading.
QLabel* section(const UiContext& ui, const QString& text, QWidget* parent);

/// Muted explanatory text.
QLabel* hint(const UiContext& ui, const QString& text, QWidget* parent);

struct Tile
{
    QString icon;
    QString label;
    std::function<void()> action;
    bool checked = false;
    bool enabled = true;
};

/// Grid of large icon tiles.
QWidget* tileGrid(const UiContext& ui, int columns, const QVector<Tile>& tiles, QWidget* parent,
                  QVector<TouchButton*>* buttons = nullptr);

/// Rounded action button.
TouchButton* pill(const UiContext& ui, const QString& icon, const QString& text, QWidget* parent,
                  std::function<void()> action, bool primary = false);

/// Grid of mutually exclusive pills (the current one is highlighted); onPick gets the index.
QWidget* choices(const UiContext& ui, const QStringList& labels, int current, QWidget* parent,
                 std::function<void(int)> onPick, int columns = 4, QVector<TouchButton*>* buttons = nullptr);

/// Horizontal row of widgets.
QWidget* row(const UiContext& ui, const QVector<QWidget*>& widgets, QWidget* parent, bool stretchEnd = true);

} // namespace panel
} // namespace cb
