#pragma once

#include "app/ExportController.h"

#include <QWidget>

namespace cb {

struct AppServices;

/// MORE: the gateway to every less frequent tool.
class MorePanel : public QWidget
{
    Q_OBJECT
public:
    MorePanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Lesson (file) actions and recent lessons.
class LessonPanel : public QWidget
{
    Q_OBJECT
public:
    LessonPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Page backgrounds: built-in and custom templates, applied to one page, all pages or new pages.
class TemplatesPanel : public QWidget
{
    Q_OBJECT
public:
    TemplatesPanel(const AppServices& services, QWidget* parent = nullptr);
};

class ImportPanel : public QWidget
{
    Q_OBJECT
public:
    ImportPanel(const AppServices& services, QWidget* parent = nullptr);
};

class ExportPanel : public QWidget
{
    Q_OBJECT
public:
    ExportPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// PDF or PowerPoint export: current page, all pages or a page range.
class ExportPagesPanel : public QWidget
{
    Q_OBJECT
public:
    ExportPagesPanel(const AppServices& services, ExportController::Format format, QWidget* parent = nullptr);
};

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    SettingsPanel(const AppServices& services, QWidget* parent = nullptr);
};

} // namespace cb
