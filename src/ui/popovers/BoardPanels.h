#pragma once

#include <QWidget>

namespace cb {

struct AppServices;

/// INSERT popover: image, shape, text, graph, equation, table, PDF and PowerPoint import.
class InsertPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InsertPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// EDIT popover: undo, redo, cut, copy, paste, duplicate, delete, select all.
class EditPanel : public QWidget
{
    Q_OBJECT
public:
    explicit EditPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// PAGE popover: new, duplicate, clear and delete page (clearly distinguished), page size,
/// background and the page navigator.
class PageActionsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PageActionsPanel(const AppServices& services, QWidget* parent = nullptr);

    /// Shared by every entry point so there is exactly one Clear Page and one Delete Page flow.
    static void confirmClearPage(const AppServices& services);
    static void deleteCurrentPage(const AppServices& services);
};

/// Page size presets (16:9 board, 4:3, A4, A3, custom) for this page, all pages or new pages.
class PageSetupPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PageSetupPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Page background colour: white, black, light grey, blackboard, custom colour.
class BackgroundPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BackgroundPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// VIEW popover: zoom presets 25 %–250 %, zoom in/out, 100 %, fit page, fit width, full screen.
class ViewPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ViewPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Drawing scale: "board distance + unit = real distance + unit".
class ScalePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ScalePanel(const AppServices& services, QWidget* parent = nullptr);
};

} // namespace cb
