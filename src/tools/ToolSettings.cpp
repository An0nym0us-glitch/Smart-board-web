#include "tools/ToolSettings.h"

#include <algorithm>

namespace cb {

ToolSettings::ToolSettings(QObject* parent)
    : QObject(parent)
{
    // Chalk-friendly default palette: high contrast on a dark board without being garish.
    m_palette = {
        QColor(QStringLiteral("#f5f5f0")), QColor(QStringLiteral("#ffd54f")), QColor(QStringLiteral("#ff8a65")),
        QColor(QStringLiteral("#ef5350")), QColor(QStringLiteral("#f48fb1")), QColor(QStringLiteral("#ba68c8")),
        QColor(QStringLiteral("#64b5f6")), QColor(QStringLiteral("#4dd0e1")), QColor(QStringLiteral("#81c784")),
        QColor(QStringLiteral("#c5e1a5")), QColor(QStringLiteral("#a1887f")), QColor(QStringLiteral("#212121")),
    };
}

void ToolSettings::setPenColor(const QColor& c)
{
    if (!c.isValid() || c == m_penColor)
        return;
    m_penColor = c;
    emit changed();
}

void ToolSettings::setPenStyle(StrokeStyle s)
{
    if (s == m_penStyle && !m_magicHighlighter)
        return;
    m_penStyle = s;
    m_magicHighlighter = false;
    emit changed();
}

void ToolSettings::setMagicHighlighter(bool on)
{
    if (on == m_magicHighlighter)
        return;
    m_magicHighlighter = on;
    emit changed();
}

void ToolSettings::setPenWidth(qreal w)
{
    w = std::clamp(w, 1.0, 120.0);
    qreal& target = usesHighlighterWidth() ? m_highlighterWidth : m_penWidth;
    if (qFuzzyCompare(target, w))
        return;
    target = w;
    emit changed();
}

void ToolSettings::setPressureEnabled(bool on)
{
    if (on == m_pressure)
        return;
    m_pressure = on;
    emit changed();
}

void ToolSettings::setSmoothing(qreal s)
{
    s = std::clamp(s, 0.0, 1.0);
    if (qFuzzyCompare(s + 1.0, m_smoothing + 1.0))
        return;
    m_smoothing = s;
    emit changed();
}

void ToolSettings::setShapeRecognition(bool on)
{
    if (on == m_shapeRecognition)
        return;
    m_shapeRecognition = on;
    emit changed();
}

InkStyle ToolSettings::ink() const
{
    InkStyle ink;
    ink.color = m_penColor;
    ink.width = penWidth();
    ink.style = m_magicHighlighter ? StrokeStyle::Highlighter : m_penStyle;
    ink.pressure = m_pressure && ink.style == StrokeStyle::Pen;
    return ink;
}

void ToolSettings::setPalette(const QVector<QColor>& palette)
{
    if (palette.isEmpty())
        return;
    m_palette = palette;
    emit changed();
}

void ToolSettings::setEraserMode(EraserMode m)
{
    if (m == m_eraserMode)
        return;
    m_eraserMode = m;
    emit changed();
}

void ToolSettings::setEraserSize(qreal s)
{
    s = std::clamp(s, 12.0, 240.0);
    if (qFuzzyCompare(s, m_eraserSize))
        return;
    m_eraserSize = s;
    emit changed();
}

void ToolSettings::setSelectMode(SelectMode m)
{
    if (m == m_selectMode)
        return;
    m_selectMode = m;
    emit changed();
}

void ToolSettings::setMultiSelect(bool on)
{
    if (on == m_multiSelect)
        return;
    m_multiSelect = on;
    emit changed();
}

void ToolSettings::setShapeKind(ShapeKind kind)
{
    if (kind == m_shapeKind)
        return;
    m_shapeKind = kind;
    emit changed();
}

void ToolSettings::setShapeFill(ShapeFill fill)
{
    if (fill == m_shapeFill)
        return;
    m_shapeFill = fill;
    emit changed();
}

void ToolSettings::setPolygonSides(int sides)
{
    sides = std::clamp(sides, 3, 12);
    if (sides == m_polygonSides)
        return;
    m_polygonSides = sides;
    emit changed();
}

ShapeStyle ToolSettings::shapeStyle() const
{
    ShapeStyle style;
    style.stroke = m_penColor;
    style.width = std::min(m_penWidth, 24.0);
    style.dashed = m_penStyle == StrokeStyle::Dashed;
    QColor fill = m_penColor;
    switch (m_shapeFill) {
    case ShapeFill::None: fill.setAlpha(0); break;
    case ShapeFill::Tint: fill.setAlpha(60); break;
    case ShapeFill::Solid: fill.setAlpha(255); break;
    }
    style.fill = fill;
    return style;
}

void ToolSettings::setTextFormat(const TextFormat& format)
{
    m_textFormat = format;
    emit changed();
}

void ToolSettings::setMeasureKind(MeasureKind kind)
{
    if (kind == m_measureKind)
        return;
    m_measureKind = kind;
    emit changed();
}

void ToolSettings::setConstructKind(ConstructKind kind)
{
    if (kind == m_constructKind)
        return;
    m_constructKind = kind;
    emit changed();
}

void ToolSettings::setShowCoordinates(bool on)
{
    if (on == m_showCoordinates)
        return;
    m_showCoordinates = on;
    emit changed();
}

void ToolSettings::setSnapToGrid(bool on)
{
    if (on == m_snapToGrid)
        return;
    m_snapToGrid = on;
    emit changed();
}

} // namespace cb
