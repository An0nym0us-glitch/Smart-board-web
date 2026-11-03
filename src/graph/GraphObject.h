#pragma once

#include "document/DocumentObject.h"
#include "math/CoordinateSystem.h"
#include "math/Expression.h"

#include <QPolygonF>

#include <vector>

namespace cb {

struct GraphFunction
{
    QString expression;   ///< as typed, e.g. "a*sin(bx)+c" or "f(x) = x^2"
    QColor color;
    bool visible = true;
};

struct GraphParameter
{
    QString name;
    double value = 1.0;
    double min = -10.0;
    double max = 10.0;
};

struct GraphPoint
{
    QPointF pos; ///< math coordinates
    QString label;
};

struct GraphVector
{
    QPointF from; ///< math coordinates
    QPointF to;
};

/// Cartesian graph with axes, grid, functions with parameters (sliders), points and vectors.
///
/// The graph exposes its own CoordinateSystem, so measurements, points and constructions placed
/// inside it automatically use graph units.
class GraphObject final : public DocumentObject
{
public:
    GraphObject();
    static std::unique_ptr<GraphObject> create(const QPointF& center, const QSizeF& size);

    // Functions
    const std::vector<GraphFunction>& functions() const { return m_functions; }
    int addFunction(const QString& expression, const QColor& color);
    void setFunctionExpression(int index, const QString& expression);
    void setFunctionVisible(int index, bool visible);
    void removeFunction(int index);
    /// Compile error of a function ("" when valid).
    QString functionError(int index) const;
    /// Evaluates function index at x with the current parameters.
    double evaluate(int index, double x) const;

    // Parameters (a, b, c ... any letter other than x used by the functions)
    const std::vector<GraphParameter>& parameters() const { return m_parameters; }
    void setParameterValue(const QString& name, double value);
    void setParameterRange(const QString& name, double min, double max);
    /// Creates parameters for new letters and drops unused ones.
    void syncParameters();

    // Points and vectors
    const std::vector<GraphPoint>& points() const { return m_points; }
    void addPoint(const QPointF& mathPos, const QString& label = QString());
    void removePoint(int index);
    int pointAt(const QPointF& local, qreal tolerance) const;
    const std::vector<GraphVector>& vectors() const { return m_vectors; }
    void addVector(const QPointF& from, const QPointF& to);
    void clearPointsAndVectors();

    // View window
    QRectF window() const { return m_window; } ///< x: left..right, y: bottom..top stored as QRectF(xMin, yMin, w, h)
    void setWindow(const QRectF& window);
    void panLocal(const QPointF& localDelta);
    void zoomAtLocal(const QPointF& localPos, qreal factor);
    void resetView();
    bool showGrid() const { return m_grid; }
    void setShowGrid(bool on);

    QPointF localToMath(const QPointF& local) const;
    QPointF mathToLocal(const QPointF& math) const;
    QSizeF size() const { return m_size; }

    QRectF localBounds() const override;
    qreal outlineMargin() const override { return 2.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool canRotate() const override { return false; }
    bool isEditable() const override { return true; }
    const CoordinateSystem* mathCoordinateSystem() const override;
    std::unique_ptr<DocumentObject> clone() const override;

    static QColor defaultColor(int index);

protected:
    bool hitTestLocal(const QPointF& local, qreal tolerance) const override;
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    GraphObject(const GraphObject&) = default;
    void invalidateCurves() const { m_curvesValid = false; }
    void rebuildCurves() const;
    const math::Expression& compiled(int index) const;
    std::vector<double> slotValues(const math::Expression& e, double x) const;
    void paintGridAndAxes(QPainter& p) const;

    QSizeF m_size{720, 540};
    QRectF m_window{-10, -7.5, 20, 15};
    bool m_grid = true;
    std::vector<GraphFunction> m_functions;
    std::vector<GraphParameter> m_parameters;
    std::vector<GraphPoint> m_points;
    std::vector<GraphVector> m_vectors;

    mutable std::vector<math::Expression> m_compiled;
    mutable std::vector<QString> m_errors;
    mutable bool m_compiledValid = false;
    mutable std::vector<std::vector<QPolygonF>> m_curves;
    mutable bool m_curvesValid = false;
    mutable CoordinateSystem m_coords;
};

} // namespace cb
