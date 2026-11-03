#pragma once

#include <QWidget>

namespace cb {

struct AppServices;
struct TextFormat;

/// PEN popover: thickness, style (pen, highlighter, dashed, dotted), colour, pressure, shape recognition.
class PenPanel : public QWidget
{
    Q_OBJECT
public:
    PenPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// ERASER popover: stroke / object / area modes, size, clear page.
class EraserPanel : public QWidget
{
    Q_OBJECT
public:
    EraserPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// SELECT popover: rectangle or lasso, multi-select, select all, paste.
class SelectPanel : public QWidget
{
    Q_OBJECT
public:
    SelectPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Shapes popover: shape kinds, fill, polygon sides.
class ShapesPanel : public QWidget
{
    Q_OBJECT
public:
    ShapesPanel(const AppServices& services, QWidget* parent = nullptr);
};

/// Text popover: size, bold / italic / underline, alignment, colour.
class TextPanel : public QWidget
{
    Q_OBJECT
public:
    TextPanel(const AppServices& services, QWidget* parent = nullptr);
    /// Applies a format to the text being edited or the selected text objects, and to new text.
    static void applyFormat(const AppServices& services, const TextFormat& format);
};

/// Colour picker used by the selection bar.
class ColorPanel : public QWidget
{
    Q_OBJECT
public:
    ColorPanel(const AppServices& services, QWidget* parent = nullptr);
};

} // namespace cb
