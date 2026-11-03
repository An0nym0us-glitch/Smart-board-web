#include "graph/GraphObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"
#include "document/ShapeObject.h"
#include "geometry/Label.h"

#include <QFont>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cb {

namespace {
const QColor kPanel(14, 19, 17, 248);
const QColor kBorder(255, 255, 255, 70);
const QColor kGrid(255, 255, 255, 26);
const QColor kGridMajor(255, 255, 255, 48);
const QColor kAxis(235, 240, 235, 210);
const QColor kLabel(220, 226, 222, 230);
} // namespace

QColor GraphObject::defaultColor(int index)
{
    static const QColor colors[] = {QColor(79, 195, 247), QColor(255, 183, 77), QColor(129, 199, 132),
                                    QColor(240, 98, 146),  QColor(186, 104, 200), QColor(255, 241, 118)};
    return colors[static_cast<size_t>(std::abs(index)) % (sizeof(colors) / sizeof(colors[0]))];
}

GraphObject::GraphObject()
    : DocumentObject(ObjectType::Graph)
{
}

std::unique_ptr<GraphObject> GraphObject::create(const QPointF& center, const QSizeF& size)
{
    auto g = std::make_unique<GraphObject>();
    g->m_size = size;
    g->setPosition(center);
    g->resetView();
    return g;
}

// ------------------------------------------------------------------------------------ functions

int GraphObject::addFunction(const QString& expression, const QColor& color)
{
    m_functions.push_back({expression, color.isValid() ? color : defaultColor(static_cast<int>(m_functions.size())), true});
    m_compiledValid = false;
    invalidateCurves();
    syncParameters();
    return static_cast<int>(m_functions.size()) - 1;
}

void GraphObject::setFunctionExpression(int index, const QString& expression)
{
    if (index < 0 || index >= static_cast<int>(m_functions.size()))
        return;
    m_functions[static_cast<size_t>(index)].expression = expression;
    m_compiledValid = false;
    invalidateCurves();
    syncParameters();
}

void GraphObject::setFunctionVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_functions.size()))
        return;
    m_functions[static_cast<size_t>(index)].visible = visible;
    invalidateCurves();
}

void GraphObject::removeFunction(int index)
{
    if (index < 0 || index >= static_cast<int>(m_functions.size()))
        return;
    m_functions.erase(m_functions.begin() + index);
    m_compiledValid = false;
    invalidateCurves();
    syncParameters();
}

const math::Expression& GraphObject::compiled(int index) const
{
    if (!m_compiledValid) {
        m_compiled.clear();
        m_errors.clear();
        for (const GraphFunction& f : m_functions) {
            QString err;
            const QString body = math::Expression::stripDefinition(f.expression);
            m_compiled.push_back(body.trimmed().isEmpty() ? math::Expression() : math::Expression::compile(body, &err));
            m_errors.push_back(body.trimmed().isEmpty() ? QString() : err);
        }
        m_compiledValid = true;
    }
    return m_compiled[static_cast<size_t>(index)];
}

QString GraphObject::functionError(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_functions.size()))
        return QString();
    compiled(index);
    return m_errors[static_cast<size_t>(index)];
}

std::vector<double> GraphObject::slotValues(const math::Expression& e, double x) const
{
    std::vector<double> values(static_cast<size_t>(e.variables().size()), 1.0);
    for (int i = 0; i < e.variables().size(); ++i) {
        const QString& name = e.variables()[i];
        if (name == QLatin1String("x")) {
            values[static_cast<size_t>(i)] = x;
            continue;
        }
        for (const GraphParameter& p : m_parameters)
            if (p.name == name)
                values[static_cast<size_t>(i)] = p.value;
    }
    return values;
}

double GraphObject::evaluate(int index, double x) const
{
    if (index < 0 || index >= static_cast<int>(m_functions.size()))
        return std::nan("");
    const math::Expression& e = compiled(index);
    if (!e.isValid())
        return std::nan("");
    const std::vector<double> v = slotValues(e, x);
    return e.evaluate(v.data(), static_cast<int>(v.size()));
}

// ----------------------------------------------------------------------------------- parameters

void GraphObject::setParameterValue(const QString& name, double value)
{
    for (GraphParameter& p : m_parameters) {
        if (p.name == name) {
            p.value = std::clamp(value, p.min, p.max);
            invalidateCurves();
        }
    }
}

void GraphObject::setParameterRange(const QString& name, double min, double max)
{
    if (max <= min)
        return;
    for (GraphParameter& p : m_parameters) {
        if (p.name == name) {
            p.min = min;
            p.max = max;
            p.value = std::clamp(p.value, min, max);
            invalidateCurves();
        }
    }
}

void GraphObject::syncParameters()
{
    QStringList used;
    for (int i = 0; i < static_cast<int>(m_functions.size()); ++i) {
        const math::Expression& e = compiled(i);
        for (const QString& v : e.variables())
            if (v != QLatin1String("x") && !used.contains(v))
                used << v;
    }
    std::sort(used.begin(), used.end());
    std::vector<GraphParameter> params;
    for (const QString& name : used) {
        auto it = std::find_if(m_parameters.begin(), m_parameters.end(), [&](const GraphParameter& p) { return p.name == name; });
        params.push_back(it != m_parameters.end() ? *it : GraphParameter{name, 1.0, -10.0, 10.0});
    }
    m_parameters = params;
    invalidateCurves();
}

// ------------------------------------------------------------------------------ points, vectors

void GraphObject::addPoint(const QPointF& mathPos, const QString& label)
{
    m_points.push_back({mathPos, label});
}

void GraphObject::removePoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_points.size()))
        m_points.erase(m_points.begin() + index);
}

int GraphObject::pointAt(const QPointF& local, qreal tolerance) const
{
    for (int i = static_cast<int>(m_points.size()) - 1; i >= 0; --i)
        if (geom::distance(mathToLocal(m_points[static_cast<size_t>(i)].pos), local) <= tolerance)
            return i;
    return -1;
}

void GraphObject::addVector(const QPointF& from, const QPointF& to)
{
    m_vectors.push_back({from, to});
}

void GraphObject::clearPointsAndVectors()
{
    m_points.clear();
    m_vectors.clear();
}

// ----------------------------------------------------------------------------------------- view

void GraphObject::setWindow(const QRectF& window)
{
    if (window.width() < 1e-6 || window.height() < 1e-6 || window.width() > 1e7 || window.height() > 1e7)
        return;
    m_window = window;
    invalidateCurves();
}

void GraphObject::panLocal(const QPointF& localDelta)
{
    const double dx = -localDelta.x() / m_size.width() * m_window.width();
    const double dy = localDelta.y() / m_size.height() * m_window.height();
    setWindow(m_window.translated(dx, dy));
}

void GraphObject::zoomAtLocal(const QPointF& localPos, qreal factor)
{
    if (factor <= 0)
        return;
    const QPointF anchor = localToMath(localPos);
    const double w = m_window.width() / factor;
    const double h = m_window.height() / factor;
    const double fx = (anchor.x() - m_window.left()) / m_window.width();
    const double fy = (anchor.y() - m_window.top()) / m_window.height();
    setWindow(QRectF(anchor.x() - fx * w, anchor.y() - fy * h, w, h));
}

void GraphObject::resetView()
{
    // 1 unit = the same number of pixels on both axes, 20 units wide.
    const double w = 20.0;
    const double h = w * m_size.height() / m_size.width();
    setWindow(QRectF(-w / 2, -h / 2, w, h));
}

void GraphObject::setShowGrid(bool on)
{
    m_grid = on;
}

QPointF GraphObject::localToMath(const QPointF& l) const
{
    const double mx = m_window.left() + (l.x() + m_size.width() / 2) / m_size.width() * m_window.width();
    const double my = m_window.top() + (m_size.height() / 2 - l.y()) / m_size.height() * m_window.height();
    return QPointF(mx, my);
}

QPointF GraphObject::mathToLocal(const QPointF& m) const
{
    const double lx = (m.x() - m_window.left()) / m_window.width() * m_size.width() - m_size.width() / 2;
    const double ly = m_size.height() / 2 - (m.y() - m_window.top()) / m_window.height() * m_size.height();
    return QPointF(lx, ly);
}

const CoordinateSystem* GraphObject::mathCoordinateSystem() const
{
    const QTransform localToMathT(m_window.width() / m_size.width(), 0, 0, -m_window.height() / m_size.height(),
                                  m_window.left() + m_window.width() / 2, m_window.top() + m_window.height() / 2);
    m_coords = CoordinateSystem::fromTransform(transform().inverted() * localToMathT, QString());
    return &m_coords;
}

QRectF GraphObject::localBounds() const
{
    return QRectF(-m_size.width() / 2, -m_size.height() / 2, m_size.width(), m_size.height());
}

bool GraphObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    return geom::inflated(localBounds(), tolerance).contains(local);
}

void GraphObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    // Keep the scale (units per pixel): resizing reveals more of the plane.
    const QPointF c = m_window.center();
    const double w = m_window.width() * newSize.width() / m_size.width();
    const double h = m_window.height() * newSize.height() / m_size.height();
    m_size = QSizeF(std::max(120.0, newSize.width()), std::max(90.0, newSize.height()));
    m_window = QRectF(c.x() - w / 2, c.y() - h / 2, w, h);
    invalidateCurves();
}

// ------------------------------------------------------------------------------------ rendering

void GraphObject::rebuildCurves() const
{
    m_curves.assign(m_functions.size(), {});
    const double h = m_size.height();
    const int n = std::max(240, static_cast<int>(m_size.width() * 1.5));
    for (int i = 0; i < static_cast<int>(m_functions.size()); ++i) {
        if (!m_functions[static_cast<size_t>(i)].visible)
            continue;
        const math::Expression& e = compiled(i);
        if (!e.isValid())
            continue;
        std::vector<double> values = slotValues(e, 0.0);
        const int xSlot = e.slotOf(QStringLiteral("x"));
        std::vector<QPolygonF>& out = m_curves[static_cast<size_t>(i)];
        QPolygonF current;
        double prevY = 0.0;
        auto flush = [&]() {
            if (current.size() >= 2)
                out.push_back(current);
            current.clear();
        };
        for (int k = 0; k <= n; ++k) {
            const double x = m_window.left() + m_window.width() * k / n;
            if (xSlot >= 0)
                values[static_cast<size_t>(xSlot)] = x;
            const double y = e.evaluate(values.data(), static_cast<int>(values.size()));
            if (!std::isfinite(y)) {
                flush();
                continue;
            }
            QPointF local = mathToLocal(QPointF(x, y));
            if (std::abs(local.y()) > 20 * h) {
                flush();
                continue;
            }
            if (!current.isEmpty() && std::abs(local.y() - prevY) > 1.5 * h)
                flush();
            local.setY(std::clamp(local.y(), -3 * h, 3 * h));
            current << local;
            prevY = local.y();
        }
        flush();
    }
    m_curvesValid = true;
}

void GraphObject::paintGridAndAxes(QPainter& p) const
{
    const QRectF r = localBounds();
    const double unitsPerPx = m_window.width() / m_size.width();
    const double step = geom::niceNumber(unitsPerPx * 48.0, true);
    const double yUnitsPerPx = m_window.height() / m_size.height();
    const double yStep = geom::niceNumber(yUnitsPerPx * 48.0, true);

    QFont f = p.font();
    f.setPixelSize(14);
    p.setFont(f);
    const QPointF origin = mathToLocal(QPointF(0, 0));
    const bool xAxisVisible = origin.y() >= r.top() && origin.y() <= r.bottom();
    const bool yAxisVisible = origin.x() >= r.left() && origin.x() <= r.right();
    const qreal labelY = xAxisVisible ? origin.y() : r.bottom() - 18;
    const qreal labelX = yAxisVisible ? origin.x() : r.left() + 4;

    for (double x = std::ceil(m_window.left() / step) * step; x <= m_window.right(); x += step) {
        const qreal lx = mathToLocal(QPointF(x, 0)).x();
        const bool major = std::abs(std::fmod(std::round(x / step), 5.0)) < 0.5;
        if (m_grid) {
            QPen pen(major ? kGridMajor : kGrid, 1.0);
            pen.setCosmetic(true);
            p.setPen(pen);
            p.drawLine(QPointF(lx, r.top()), QPointF(lx, r.bottom()));
        }
        if (std::abs(x) > step * 0.5) {
            p.setPen(kLabel);
            p.drawText(QRectF(lx - 40, labelY + 3, 80, 18), Qt::AlignHCenter | Qt::AlignTop, geom::formatNumber(x, 3));
        }
    }
    for (double y = std::ceil(m_window.top() / yStep) * yStep; y <= m_window.bottom(); y += yStep) {
        const qreal ly = mathToLocal(QPointF(0, y)).y();
        const bool major = std::abs(std::fmod(std::round(y / yStep), 5.0)) < 0.5;
        if (m_grid) {
            QPen pen(major ? kGridMajor : kGrid, 1.0);
            pen.setCosmetic(true);
            p.setPen(pen);
            p.drawLine(QPointF(r.left(), ly), QPointF(r.right(), ly));
        }
        if (std::abs(y) > yStep * 0.5) {
            p.setPen(kLabel);
            p.drawText(QRectF(labelX - 84, ly - 9, 80, 18), Qt::AlignRight | Qt::AlignVCenter, geom::formatNumber(y, 3));
        }
    }
    QPen axis(kAxis, 2.0);
    p.setPen(axis);
    if (xAxisVisible) {
        p.drawLine(QPointF(r.left(), origin.y()), QPointF(r.right(), origin.y()));
        ShapeObject::paintArrow(p, QPointF(r.right() - 20, origin.y()), QPointF(r.right() - 2, origin.y()), axis, false, true);
        p.setPen(kAxis);
        p.drawText(QRectF(r.right() - 26, origin.y() - 26, 20, 20), Qt::AlignCenter, QStringLiteral("x"));
    }
    if (yAxisVisible) {
        p.setPen(axis);
        p.drawLine(QPointF(origin.x(), r.top()), QPointF(origin.x(), r.bottom()));
        ShapeObject::paintArrow(p, QPointF(origin.x(), r.top() + 20), QPointF(origin.x(), r.top() + 2), axis, false, true);
        p.setPen(kAxis);
        p.drawText(QRectF(origin.x() + 6, r.top() + 2, 20, 20), Qt::AlignCenter, QStringLiteral("y"));
    }
    if (xAxisVisible && yAxisVisible) {
        p.setPen(kLabel);
        p.drawText(QRectF(origin.x() - 22, origin.y() + 2, 18, 18), Qt::AlignRight | Qt::AlignTop, QStringLiteral("0"));
    }
}

void GraphObject::paint(QPainter& p, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    const QRectF r = localBounds();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(kBorder, 1.5));
    p.setBrush(kPanel);
    p.drawRoundedRect(r, 10, 10);
    p.setClipRect(r.adjusted(1, 1, -1, -1));

    paintGridAndAxes(p);

    if (!m_curvesValid)
        rebuildCurves();
    for (size_t i = 0; i < m_functions.size() && i < m_curves.size(); ++i) {
        QPen pen(m_functions[i].color, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        for (const QPolygonF& poly : m_curves[i])
            p.drawPolyline(poly);
    }

    for (const GraphVector& v : m_vectors) {
        const QPen pen(QColor(255, 213, 79), 3.0, Qt::SolidLine, Qt::RoundCap);
        ShapeObject::paintArrow(p, mathToLocal(v.from), mathToLocal(v.to), pen, false, true);
        const QPointF d = v.to - v.from;
        paintValueLabel(p, geom::midpoint(mathToLocal(v.from), mathToLocal(v.to)) + QPointF(0, -22),
                        QStringLiteral("⟨%1, %2⟩").arg(geom::formatNumber(d.x(), 2), geom::formatNumber(d.y(), 2)),
                        QColor(255, 224, 130), 0.0, 16);
    }
    for (const GraphPoint& pt : m_points) {
        const QPointF l = mathToLocal(pt.pos);
        p.setPen(QPen(QColor(15, 15, 15), 1.5));
        p.setBrush(QColor(255, 255, 255));
        p.drawEllipse(l, 6.5, 6.5);
        const QString coords = QStringLiteral("(%1, %2)").arg(geom::formatNumber(pt.pos.x(), 2), geom::formatNumber(pt.pos.y(), 2));
        paintValueLabel(p, l + QPointF(0, -24), pt.label.isEmpty() ? coords : pt.label + QLatin1Char(' ') + coords,
                        QColor(255, 255, 255), 0.0, 15);
    }

    // Legend: functions and parameter values.
    QFont legendFont = p.font();
    legendFont.setPixelSize(17);
    legendFont.setBold(true);
    p.setFont(legendFont);
    const QFontMetricsF fm(legendFont);
    qreal y = r.top() + 10;
    const char* names = "fghpqrs";
    for (size_t i = 0; i < m_functions.size(); ++i) {
        const GraphFunction& f = m_functions[i];
        if (f.expression.trimmed().isEmpty())
            continue;
        QString defName;
        const QString body = math::Expression::stripDefinition(f.expression, &defName);
        const QChar fname = i < 7 ? QLatin1Char(names[i]) : QLatin1Char('f');
        const QString text = (defName.isEmpty() || defName == QLatin1String("y") ? QString(fname) : defName)
            + QStringLiteral("(x) = ") + body.trimmed();
        const bool error = !functionError(static_cast<int>(i)).isEmpty();
        const qreal w = fm.horizontalAdvance(text) + 36;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 14, 12, 200));
        p.drawRoundedRect(QRectF(r.left() + 8, y, w, fm.height() + 8), 6, 6);
        p.setBrush(f.color);
        p.drawEllipse(QPointF(r.left() + 22, y + (fm.height() + 8) / 2), 6, 6);
        p.setPen(error ? QColor(239, 106, 100) : (f.visible ? f.color.lighter(115) : kLabel));
        p.drawText(QRectF(r.left() + 34, y + 4, w, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += fm.height() + 12;
    }
    if (!m_parameters.empty()) {
        QStringList parts;
        for (const GraphParameter& prm : m_parameters)
            parts << prm.name + QStringLiteral(" = ") + geom::formatNumber(prm.value, 2);
        const QString text = parts.join(QStringLiteral("   "));
        const qreal w = fm.horizontalAdvance(text) + 20;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 14, 12, 200));
        p.drawRoundedRect(QRectF(r.left() + 8, y, w, fm.height() + 8), 6, 6);
        p.setPen(QColor(255, 224, 130));
        p.drawText(QRectF(r.left() + 18, y + 4, w, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    p.restore();
}

std::unique_ptr<DocumentObject> GraphObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new GraphObject(*this));
}

// ---------------------------------------------------------------------------------------- JSON

void GraphObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("size"), json::fromSize(m_size));
    obj.insert(QStringLiteral("window"), QJsonArray{m_window.left(), m_window.top(), m_window.width(), m_window.height()});
    obj.insert(QStringLiteral("grid"), m_grid);
    QJsonArray functions;
    for (const GraphFunction& f : m_functions) {
        QJsonObject o;
        o.insert(QStringLiteral("expr"), f.expression);
        o.insert(QStringLiteral("color"), json::fromColor(f.color));
        o.insert(QStringLiteral("visible"), f.visible);
        functions.append(o);
    }
    obj.insert(QStringLiteral("functions"), functions);
    QJsonArray params;
    for (const GraphParameter& prm : m_parameters) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), prm.name);
        o.insert(QStringLiteral("value"), prm.value);
        o.insert(QStringLiteral("min"), prm.min);
        o.insert(QStringLiteral("max"), prm.max);
        params.append(o);
    }
    obj.insert(QStringLiteral("parameters"), params);
    QJsonArray points;
    for (const GraphPoint& pt : m_points)
        points.append(QJsonObject{{QStringLiteral("x"), pt.pos.x()}, {QStringLiteral("y"), pt.pos.y()},
                                  {QStringLiteral("label"), pt.label}});
    obj.insert(QStringLiteral("points"), points);
    QJsonArray vectors;
    for (const GraphVector& v : m_vectors)
        vectors.append(QJsonArray{v.from.x(), v.from.y(), v.to.x(), v.to.y()});
    obj.insert(QStringLiteral("vectors"), vectors);
}

bool GraphObject::readProperties(const QJsonObject& obj)
{
    m_size = json::toSize(obj.value(QStringLiteral("size")), QSizeF(720, 540));
    const QJsonArray w = obj.value(QStringLiteral("window")).toArray();
    if (w.size() == 4)
        m_window = QRectF(w.at(0).toDouble(), w.at(1).toDouble(), w.at(2).toDouble(), w.at(3).toDouble());
    if (m_window.width() <= 0 || m_window.height() <= 0)
        m_window = QRectF(-10, -7.5, 20, 15);
    m_grid = obj.value(QStringLiteral("grid")).toBool(true);
    m_functions.clear();
    for (const QJsonValue& v : obj.value(QStringLiteral("functions")).toArray()) {
        const QJsonObject o = v.toObject();
        m_functions.push_back({o.value(QStringLiteral("expr")).toString(),
                               json::toColor(o.value(QStringLiteral("color")), defaultColor(static_cast<int>(m_functions.size()))),
                               o.value(QStringLiteral("visible")).toBool(true)});
    }
    m_parameters.clear();
    for (const QJsonValue& v : obj.value(QStringLiteral("parameters")).toArray()) {
        const QJsonObject o = v.toObject();
        GraphParameter prm;
        prm.name = o.value(QStringLiteral("name")).toString();
        prm.min = o.value(QStringLiteral("min")).toDouble(-10);
        prm.max = o.value(QStringLiteral("max")).toDouble(10);
        prm.value = o.value(QStringLiteral("value")).toDouble(1);
        if (!prm.name.isEmpty())
            m_parameters.push_back(prm);
    }
    m_points.clear();
    for (const QJsonValue& v : obj.value(QStringLiteral("points")).toArray()) {
        const QJsonObject o = v.toObject();
        m_points.push_back({QPointF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble()),
                            o.value(QStringLiteral("label")).toString()});
    }
    m_vectors.clear();
    for (const QJsonValue& v : obj.value(QStringLiteral("vectors")).toArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() == 4)
            m_vectors.push_back({QPointF(a.at(0).toDouble(), a.at(1).toDouble()), QPointF(a.at(2).toDouble(), a.at(3).toDouble())});
    }
    m_compiledValid = false;
    m_curvesValid = false;
    return true;
}

} // namespace cb
