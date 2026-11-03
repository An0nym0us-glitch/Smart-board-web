#include "ui/popovers/PropertiesPanel.h"

#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "core/Geometry.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "geometry/Precision.h"
#include "math/CoordinateSystem.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/NumberField.h"

#include <cmath>

namespace cb {

namespace {
enum class Figure { None, Line, Vector, Angle, Slope, Area, Box, Point };

Figure figureOf(const DocumentObject& o)
{
    switch (o.type()) {
    case ObjectType::Measurement:
        switch (static_cast<const MeasurementObject&>(o).kind()) {
        case MeasureKind::Distance: return Figure::Line;
        case MeasureKind::Angle: return Figure::Angle;
        case MeasureKind::Slope: return Figure::Slope;
        case MeasureKind::Area: return Figure::Area;
        }
        return Figure::None;
    case ObjectType::Geometry:
        switch (static_cast<const GeometryObject&>(o).kind()) {
        case ConstructKind::Point: return Figure::Point;
        case ConstructKind::Vector: return Figure::Vector;
        default: return Figure::Line;
        }
    case ObjectType::Shape:
        return isLineShape(static_cast<const ShapeObject&>(o).kind()) ? Figure::Line : Figure::Box;
    default:
        return Figure::None;
    }
}

const QVector<double>& quickAngles()
{
    static const QVector<double> a = {0, 30, 45, 60, 90, 120, 135, 180};
    return a;
}
} // namespace

bool PropertiesPanel::supports(const DocumentObject& object)
{
    return figureOf(object) != Figure::None;
}

QString PropertiesPanel::titleFor(const DocumentObject& object)
{
    switch (figureOf(object)) {
    case Figure::Line: return tr("Line");
    case Figure::Vector: return tr("Vector");
    case Figure::Angle: return tr("Angle");
    case Figure::Slope: return tr("Slope");
    case Figure::Area: return tr("Area");
    case Figure::Box: return shapeKindLabel(static_cast<const ShapeObject&>(object).kind());
    case Figure::Point: return tr("Point");
    case Figure::None: break;
    }
    return tr("Properties");
}

PropertiesPanel::PropertiesPanel(const AppServices& s, const ObjectId& id, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    Page* page = s.doc.currentPage();
    DocumentObject* object = page ? page->object(id) : nullptr;
    if (!object || !supports(*object)) {
        layout->addWidget(panel::hint(ui, tr("Select a line, vector, angle, slope, area, point or shape."), this));
        return;
    }
    const Figure figure = figureOf(*object);
    const CoordinateSystem cs = precision::coordinateSystemFor(s.doc, page, *object);
    const bool graphUnits = cs.unitLabel().isEmpty();
    const QString boardUnit = graphUnits ? tr("units") : cs.unitLabel();
    const bool scaled = cs.usesScale();
    const QString realUnit = cs.realUnitLabel();
    const QVector<QPointF> pts = precision::definingPoints(*object);

    // Applies new defining points as one undoable command, then refreshes all values.
    auto commit = [sp, id](const QVector<QPointF>& newPts, const QString& text) {
        Page* p = sp->doc.currentPage();
        if (!p)
            return;
        precision::applyPoints(sp->doc, *p, id, newPts, text);
        sp->popovers.refresh();
    };
    auto field = [&](const QString& label, const QString& unit, double value, double step, int decimals) {
        auto* f = new NumberField(ui, label, unit, this);
        f->setStep(step);
        f->setDecimals(decimals);
        f->setValue(value);
        layout->addWidget(f);
        return f;
    };
    auto readOnly = [&](const QString& label, const QString& unit, double value, int decimals) {
        NumberField* f = field(label, unit, value, 1, decimals);
        f->setReadOnly(true);
        return f;
    };
    auto lengthFields = [&](const QVector<QPointF>& p, const QString& label, const QString& text) {
        const double len = precision::length(p, cs);
        NumberField* board = field(scaled ? tr("%1 (board)").arg(label) : label, boardUnit, len, 0.5, 3);
        board->setRange(1e-6, 1e7);
        connect(board, &NumberField::valueEdited, this, [commit, p, cs, text](double v) {
            commit(precision::withLength(p, v, cs), text);
        });
        if (scaled) {
            NumberField* real = field(tr("%1 (real)").arg(label), realUnit, cs.toReal(len), 0.1, 4);
            real->setRange(1e-9, 1e12);
            connect(real, &NumberField::valueEdited, this, [commit, p, cs, text](double v) {
                commit(precision::withLength(p, cs.fromReal(v), cs), text);
            });
        }
    };
    auto directionFields = [&](const QVector<QPointF>& p, const QString& text) {
        NumberField* dir = field(tr("Direction"), QStringLiteral("°"), precision::direction(p, cs), 15, 3);
        dir->setRange(-360, 360);
        connect(dir, &NumberField::valueEdited, this, [commit, p, cs, text](double v) {
            commit(precision::withDirection(p, v, cs), text);
        });
    };
    auto anglePills = [&](std::function<void(double)> apply) {
        QStringList labels;
        for (double a : quickAngles())
            labels << geom::formatNumber(a, 0) + QStringLiteral("°");
        layout->addWidget(panel::choices(ui, labels, -1, this, [apply](int i) { apply(quickAngles().value(i)); }, 8));
    };

    switch (figure) {
    case Figure::Line: {
        const QString text = tr("Set length");
        lengthFields(pts, tr("Length"), text);
        directionFields(pts, tr("Set direction"));
        anglePills([commit, pts, cs](double a) { commit(precision::withDirection(pts, a, cs), tr("Set direction")); });
        break;
    }
    case Figure::Vector: {
        const double len = precision::length(pts, cs);
        NumberField* magnitude = field(tr("Magnitude"), realUnit.isEmpty() ? boardUnit : realUnit, cs.toReal(len), scaled ? 0.1 : 0.5, 4);
        magnitude->setRange(1e-9, 1e12);
        connect(magnitude, &NumberField::valueEdited, this, [commit, pts, cs](double v) {
            commit(precision::withLength(pts, cs.fromReal(v), cs), tr("Set magnitude"));
        });
        directionFields(pts, tr("Set direction"));
        anglePills([commit, pts, cs](double a) { commit(precision::withDirection(pts, a, cs), tr("Set direction")); });
        const QPointF d = cs.toMath(pts[1]) - cs.toMath(pts[0]);
        readOnly(tr("Δx"), realUnit.isEmpty() ? boardUnit : realUnit, cs.toReal(d.x()), 4);
        readOnly(tr("Δy"), realUnit.isEmpty() ? boardUnit : realUnit, cs.toReal(d.y()), 4);
        if (scaled)
            readOnly(tr("Board length"), boardUnit, len, 3);
        break;
    }
    case Figure::Angle: {
        NumberField* angle = field(tr("Angle"), QStringLiteral("°"), std::max(0.0, precision::angle(pts, cs)), 1, 3);
        angle->setRange(0, 360);
        connect(angle, &NumberField::valueEdited, this, [commit, pts, cs](double v) {
            commit(precision::withAngle(pts, v, cs), tr("Set angle"));
        });
        anglePills([commit, pts, cs](double a) { commit(precision::withAngle(pts, a, cs), tr("Set angle")); });
        break;
    }
    case Figure::Slope: {
        const precision::SlopeValues v = precision::slopeValues(pts, cs);
        const QString unit = realUnit.isEmpty() ? boardUnit : realUnit;
        NumberField* rise = field(tr("Rise"), unit, cs.toReal(v.rise), scaled ? 0.1 : 0.5, 4);
        NumberField* run = field(tr("Run"), unit, cs.toReal(v.run), scaled ? 0.1 : 0.5, 4);
        connect(rise, &NumberField::valueEdited, this, [commit, pts, cs, v](double r) {
            commit(precision::withRiseRun(pts, cs.fromReal(r), v.run, cs), tr("Set rise"));
        });
        connect(run, &NumberField::valueEdited, this, [commit, pts, cs, v](double r) {
            commit(precision::withRiseRun(pts, v.rise, cs.fromReal(r), cs), tr("Set run"));
        });
        NumberField* slope = field(tr("Slope"), QString(), v.slope, 0.25, 4);
        if (v.vertical) {
            slope->setReadOnly(true);
            layout->addWidget(panel::hint(ui, tr("The line is vertical: the slope is undefined."), this));
        }
        connect(slope, &NumberField::valueEdited, this, [commit, pts, cs](double m) {
            commit(precision::withSlope(pts, m, cs), tr("Set slope"));
        });
        NumberField* inclination = field(tr("Angle"), QStringLiteral("°"), v.angle, 5, 3);
        inclination->setRange(-90, 90);
        connect(inclination, &NumberField::valueEdited, this, [commit, pts, cs](double a) {
            commit(precision::withInclination(pts, a, cs), tr("Set slope angle"));
        });
        layout->addWidget(panel::hint(ui, tr("Rise and run are measured from the first point; the dashed triangle "
                                             "shows them."),
                                      this));
        break;
    }
    case Figure::Area:
    case Figure::Box: {
        precision::ShapeMetrics m;
        if (!precision::shapeMetrics(*object, cs, &m)) {
            layout->addWidget(panel::hint(ui, tr("This shape has no measurable area."), this));
            break;
        }
        const bool resizable = figure == Figure::Box && object->canResize();
        const bool circle = object->type() == ObjectType::Shape
            && static_cast<const ShapeObject*>(object)->kind() == ShapeKind::Circle;
        auto sizeField = [&](const QString& label, double value, bool widthField) {
            NumberField* board = field(scaled ? tr("%1 (board)").arg(label) : label, boardUnit, value, 0.5, 3);
            board->setRange(1e-3, 1e6);
            board->setReadOnly(!resizable);
            NumberField* real = nullptr;
            if (scaled) {
                real = field(tr("%1 (real)").arg(label), realUnit, cs.toReal(value), 0.1, 4);
                real->setRange(1e-9, 1e12);
                real->setReadOnly(!resizable);
            }
            auto apply = [sp, id, cs, m, widthField, circle](double boardValue) {
                Page* p = sp->doc.currentPage();
                if (!p)
                    return;
                const double w = circle || widthField ? boardValue : m.width;
                const double h = circle || !widthField ? boardValue : m.height;
                precision::applyShapeSize(sp->doc, *p, id, w, h, cs, tr("Set size"));
                sp->popovers.refresh();
            };
            connect(board, &NumberField::valueEdited, this, apply);
            if (real)
                connect(real, &NumberField::valueEdited, this, [apply, cs](double v) { apply(cs.fromReal(v)); });
        };
        if (circle) {
            sizeField(tr("Diameter"), m.width, true);
        } else {
            sizeField(tr("Width"), m.width, true);
            sizeField(tr("Height"), m.height, false);
        }
        const QString areaUnit = (realUnit.isEmpty() ? boardUnit : realUnit) + QStringLiteral("²");
        readOnly(tr("Area"), areaUnit, cs.toRealArea(m.area), 4);
        readOnly(tr("Perimeter"), realUnit.isEmpty() ? boardUnit : realUnit, cs.toReal(m.perimeter), 4);
        if (figure == Figure::Area)
            layout->addWidget(panel::hint(ui, tr("Drag the corner points with SELECT to change the area."), this));
        break;
    }
    case Figure::Point: {
        const QPointF m = cs.toMath(pts[0]);
        NumberField* x = field(QStringLiteral("x"), QString(), m.x(), 0.5, 4);
        NumberField* y = field(QStringLiteral("y"), QString(), m.y(), 0.5, 4);
        connect(x, &NumberField::valueEdited, this, [commit, cs, m](double v) {
            commit({cs.toPage(QPointF(v, m.y()))}, tr("Move point"));
        });
        connect(y, &NumberField::valueEdited, this, [commit, cs, m](double v) {
            commit({cs.toPage(QPointF(m.x(), v))}, tr("Move point"));
        });
        break;
    }
    case Figure::None:
        break;
    }

    // Scale information (the scale applies to board lengths, not to graph axes).
    if (graphUnits) {
        layout->addWidget(panel::hint(ui, tr("Inside a graph: values use the graph's axes."), this));
    } else if (figure != Figure::Angle && figure != Figure::Point) {
        const QString scaleText = scaled ? tr("Scale %1").arg(cs.scale().text()) : tr("No scale (1 : 1)");
        auto* change = panel::pill(ui, QStringLiteral("scale"), tr("Change scale"), this,
                                   [sp]() { sp->popovers.push(QStringLiteral("scale")); });
        auto* label = panel::hint(ui, scaleText, this);
        layout->addWidget(panel::row(ui, {label, change}, this));
    }
    layout->addWidget(panel::hint(ui, tr("Dragging still works; values here stay in sync."), this));
}

} // namespace cb
