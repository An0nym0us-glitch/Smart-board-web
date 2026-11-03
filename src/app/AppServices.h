#pragma once

#include "math/Calculator.h"
#include "tools/Tool.h"

#include <functional>

namespace cb {

struct UiContext;
class AppSettings;
class CanvasWidget;
class Document;
class EditOperations;
class ExportController;
class LessonController;
class PopoverController;
class RecognizerRegistry;
class TemplateLibrary;
class ThumbnailCache;
class Toast;
class ToolSettings;

/// References to the application services that popover panels work with. Owned by MainWindow,
/// passed explicitly (no global state).
struct AppServices
{
    const UiContext& ui;
    Document& doc;
    ToolSettings& tools;
    AppSettings& settings;
    CanvasWidget& canvas;
    LessonController& lesson;
    ExportController& exporter;
    TemplateLibrary& templates;
    ThumbnailCache& thumbnails;
    EditOperations& edit;
    Toast& toast;
    PopoverController& popovers;
    const RecognizerRegistry& recognizers;
    math::Calculator& calculator; ///< kept while the app runs (Ans, history, DEG/RAD)
    std::function<void(ToolId)> activateTool;
    std::function<void(qreal)> applyUiScale;
    /// Resizes the bottom ribbon (relative to the interface size, 1 = normal).
    std::function<void(qreal)> applyRibbonScale;
};

} // namespace cb
