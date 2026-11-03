#pragma once

#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"

#include <QColor>
#include <QObject>
#include <QVector>

namespace cb {

enum class EraserMode {
    Stroke, ///< removes whole ink strokes that are touched
    Object, ///< removes any object that is touched
    Area,   ///< erases the ink area under the eraser (splits strokes)
};

enum class SelectMode {
    Rectangle,
    Lasso,
};

enum class ShapeFill {
    None,
    Tint,
    Solid,
};

/// Shared, observable settings of the drawing tools. Owned by the main window and persisted by
/// AppSettings. Tools read these; popovers edit them.
class ToolSettings : public QObject
{
    Q_OBJECT
public:
    explicit ToolSettings(QObject* parent = nullptr);

    // Pen
    QColor penColor() const { return m_penColor; }
    void setPenColor(const QColor& c);
    StrokeStyle penStyle() const { return m_penStyle; }
    /// Selecting a style also leaves the Magic Highlighter.
    void setPenStyle(StrokeStyle s);
    /// Magic Highlighter: the pen draws temporary highlights that fade away by themselves and
    /// never become part of the document. A session choice, deliberately not persisted.
    bool magicHighlighter() const { return m_magicHighlighter; }
    void setMagicHighlighter(bool on);
    /// Width used by the current style (highlighter and Magic Highlighter share their own width).
    qreal penWidth() const { return usesHighlighterWidth() ? m_highlighterWidth : m_penWidth; }
    void setPenWidth(qreal w);
    bool pressureEnabled() const { return m_pressure; }
    void setPressureEnabled(bool on);
    qreal smoothing() const { return m_smoothing; }
    void setSmoothing(qreal s);
    bool shapeRecognition() const { return m_shapeRecognition; }
    void setShapeRecognition(bool on);
    InkStyle ink() const;
    bool usesHighlighterWidth() const { return m_magicHighlighter || m_penStyle == StrokeStyle::Highlighter; }

    const QVector<QColor>& palette() const { return m_palette; }
    void setPalette(const QVector<QColor>& palette);

    // Eraser
    EraserMode eraserMode() const { return m_eraserMode; }
    void setEraserMode(EraserMode m);
    /// Eraser diameter in view pixels (constant on screen, independent of zoom).
    qreal eraserSize() const { return m_eraserSize; }
    void setEraserSize(qreal s);

    // Select
    SelectMode selectMode() const { return m_selectMode; }
    void setSelectMode(SelectMode m);
    bool multiSelect() const { return m_multiSelect; }
    void setMultiSelect(bool on);

    // Shapes
    ShapeKind shapeKind() const { return m_shapeKind; }
    void setShapeKind(ShapeKind kind);
    ShapeFill shapeFill() const { return m_shapeFill; }
    void setShapeFill(ShapeFill fill);
    int polygonSides() const { return m_polygonSides; }
    void setPolygonSides(int sides);
    ShapeStyle shapeStyle() const;

    // Text
    const TextFormat& textFormat() const { return m_textFormat; }
    void setTextFormat(const TextFormat& format);

    // Measurement and constructions
    MeasureKind measureKind() const { return m_measureKind; }
    void setMeasureKind(MeasureKind kind);
    ConstructKind constructKind() const { return m_constructKind; }
    void setConstructKind(ConstructKind kind);
    bool showCoordinates() const { return m_showCoordinates; }
    void setShowCoordinates(bool on);
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool on);

signals:
    void changed();

private:
    QColor m_penColor = QColor(QStringLiteral("#f5f5f0"));
    StrokeStyle m_penStyle = StrokeStyle::Pen;
    bool m_magicHighlighter = false;
    qreal m_penWidth = 4.0;
    qreal m_highlighterWidth = 22.0;
    bool m_pressure = true;
    qreal m_smoothing = 0.45;
    bool m_shapeRecognition = false;
    QVector<QColor> m_palette;

    EraserMode m_eraserMode = EraserMode::Area;
    qreal m_eraserSize = 48.0;

    SelectMode m_selectMode = SelectMode::Rectangle;
    bool m_multiSelect = false;

    ShapeKind m_shapeKind = ShapeKind::Rectangle;
    ShapeFill m_shapeFill = ShapeFill::None;
    int m_polygonSides = 5;
    TextFormat m_textFormat;

    MeasureKind m_measureKind = MeasureKind::Distance;
    ConstructKind m_constructKind = ConstructKind::Point;
    bool m_showCoordinates = true;
    bool m_snapToGrid = true;
};

} // namespace cb
