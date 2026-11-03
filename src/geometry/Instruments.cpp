#include "geometry/Instruments.h"

#include "core/Geometry.h"
#include "geometry/MeasurementObject.h"

#include <QFont>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace cb {

namespace {
const QColor kPlastic(232, 238, 234, 200);
const QColor kPlasticActive(236, 244, 240, 215);
const QColor kEdge(20, 26, 24, 150);
const QColor kTick(28, 32, 30, 230);
const QColor kArm1(33, 150, 243);
const QColor kArm2(245, 124, 0);

QPen cosmetic(const QColor& c, qreal w = 1.0)
{
    QPen p(c, w);
    p.setCosmetic(true);
    return p;
}

void drawCenteredText(QPainter& p, const QPointF& center, const QString& text, int pixelSize, const QColor& color,
                      bool bold = false)
{
    QFont f = p.font();
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(center.x() - 120, center.y() - pixelSize, 240, pixelSize * 2), Qt::AlignCenter, text);
}

QString unitText(qreal pixels, qreal ppu, const QString& unit)
{
    return geom::formatNumber(pixels / ppu, 1) + QLatin1Char(' ') + unit;
}

/// Angle of a direction as seen by the class (counter-clockwise, 0..180 for lines).
qreal lineAngle(qreal rotation)
{
    qreal a = geom::normalizeDegrees(-rotation);
    if (a >= 180.0)
        a -= 180.0;
    return a;
}

void paintScale(QPainter& p, const QPointF& start, const QPointF& dir, const QPointF& inward, qreal length,
                qreal ppu, qreal zoom, bool labels)
{
    const qreal mm = ppu / 10.0;
    const bool drawMm = mm * zoom >= 3.5;
    const int labelEvery = ppu * zoom >= 26.0 ? 1 : (ppu * zoom >= 8.0 ? 5 : 10);
    const int count = static_cast<int>(std::floor(length / mm + 1e-6));
    for (int i = 0; i <= count; ++i) {
        const bool cm = i % 10 == 0;
        const bool half = i % 5 == 0;
        if (!drawMm && !half)
            continue;
        const qreal len = cm ? 24.0 : (half ? 16.0 : 9.0);
        const QPointF a = start + dir * (i * mm);
        p.setPen(cosmetic(kTick, cm ? 1.4 : 1.0));
        p.drawLine(a, a + inward * len);
        if (labels && cm && (i / 10) % labelEvery == 0)
            drawCenteredText(p, a + inward * 38.0, QString::number(i / 10), 15, kTick);
    }
}
} // namespace

std::unique_ptr<Instrument> createInstrument(Instrument::Kind kind)
{
    switch (kind) {
    case Instrument::Kind::Ruler:
        return std::make_unique<Ruler>();
    case Instrument::Kind::Protractor:
        return std::make_unique<Protractor>();
    case Instrument::Kind::SetSquare:
        return std::make_unique<SetSquare>();
    case Instrument::Kind::Compass:
        return std::make_unique<Compass>();
    }
    return nullptr;
}

// ============================================================================================ Ruler

void Ruler::setLengthUnits(qreal units)
{
    m_units = std::clamp(std::round(units), 5.0, 100.0);
}

QPainterPath Ruler::bodyShape() const
{
    const qreal len = length();
    QPainterPath path;
    path.addRoundedRect(QRectF(-len / 2, -kHeight / 2, len, kHeight), 8, 8);
    return path;
}

QPointF Ruler::closeButtonPos() const { return QPointF(-length() / 2 + 34, 20); }
QPointF Ruler::rotateHandlePos() const { return QPointF(length() / 2 - 34, 20); }

int Ruler::handleAt(const QPointF& local, qreal tol) const
{
    if (geom::distance(local, QPointF(length() / 2 + 18, 0)) <= 18 + tol)
        return kLengthHandle;
    return Instrument::handleAt(local, tol);
}

bool Ruler::beginInteraction(int handle, const QPointF& pagePos)
{
    return Instrument::beginInteraction(handle, pagePos);
}

void Ruler::updateInteraction(const QPointF& pagePos)
{
    if (m_handle != kLengthHandle) {
        Instrument::updateInteraction(pagePos);
        return;
    }
    const QPointF leftPage = toPage(QPointF(-length() / 2, 0));
    const qreal dx = toLocal(pagePos).x() - toLocal(leftPage).x();
    setLengthUnits((dx - 18 - 2 * kMargin) / m_pxPerUnit);
    m_position = leftPage + geom::rotated(QPointF(length() / 2, 0), m_rotation);
}

bool Ruler::edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const
{
    const QPointF local = toLocal(pagePos);
    const qreal half = length() / 2;
    if (local.x() < -half - tol || local.x() > half + tol)
        return false;
    const qreal edges[2] = {-kHeight / 2, kHeight / 2};
    qreal best = tol;
    bool found = false;
    for (qreal y : edges) {
        const qreal d = std::abs(local.y() - y);
        if (d <= best) {
            best = d;
            found = true;
            if (out) {
                out->kind = EdgeConstraint::Kind::Line;
                out->origin = toPage(QPointF(0, y));
                out->direction = geom::rotated(QPointF(1, 0), m_rotation);
            }
        }
    }
    return found;
}

QString Ruler::statusText() const
{
    return QStringLiteral("%1 %2 · %3°")
        .arg(geom::formatNumber(m_units, 0), m_unitLabel, geom::formatNumber(lineAngle(m_rotation), 1));
}

void Ruler::paint(QPainter& p, const InstrumentPaintContext& ctx) const
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal len = length();
    const QRectF body(-len / 2, -kHeight / 2, len, kHeight);
    p.setPen(cosmetic(kEdge, 1.2));
    p.setBrush(ctx.active ? kPlasticActive : kPlastic);
    p.drawRoundedRect(body, 8, 8);

    // Top edge: full metric scale. Bottom edge: centimetre ticks.
    paintScale(p, QPointF(-len / 2 + kMargin, -kHeight / 2), QPointF(1, 0), QPointF(0, 1), m_units * m_pxPerUnit,
               m_pxPerUnit, ctx.zoom, true);
    p.setPen(cosmetic(kTick, 1.0));
    for (int i = 0; i <= static_cast<int>(m_units); ++i) {
        const qreal x = -len / 2 + kMargin + i * m_pxPerUnit;
        p.drawLine(QPointF(x, kHeight / 2), QPointF(x, kHeight / 2 - (i % 5 == 0 ? 14 : 8)));
    }

    drawCenteredText(p, QPointF(0, 22), QStringLiteral("%1°").arg(geom::formatNumber(lineAngle(m_rotation), 1)), 16,
                     kTick, true);
    paintCloseButton(p, closeButtonPos(), 17);
    paintRotateKnob(p, rotateHandlePos(), 17);

    // Length tab.
    const QPointF tab(len / 2 + 18, 0);
    p.setPen(cosmetic(kEdge, 1.0));
    p.setBrush(QColor(40, 44, 46, 220));
    p.drawRoundedRect(QRectF(tab.x() - 10, tab.y() - 22, 20, 44), 6, 6);
    p.setPen(cosmetic(QColor(230, 230, 230), 1.4));
    for (int k = -1; k <= 1; ++k)
        p.drawLine(QPointF(tab.x() - 4, tab.y() + k * 7), QPointF(tab.x() + 4, tab.y() + k * 7));
}

// ======================================================================================= Protractor

QPainterPath Protractor::bodyShape() const
{
    const qreal R = radius() + 12;
    QPainterPath path;
    path.moveTo(-R, 26);
    path.lineTo(R, 26);
    path.lineTo(R, 0);
    path.arcTo(QRectF(-R, -R, 2 * R, 2 * R), 0, 180);
    path.closeSubpath();
    return path;
}

QPointF Protractor::closeButtonPos() const { return QPointF(-radius() + 22, 13); }
QPointF Protractor::rotateHandlePos() const { return QPointF(radius() - 22, 13); }
QPointF Protractor::stampPos() const { return QPointF(-radius() + 68, 13); }

QPointF Protractor::armKnob(qreal angle) const
{
    const qreal r = radius() + 46;
    const qreal a = geom::degToRad(angle);
    return QPointF(std::cos(a) * r, -std::sin(a) * r);
}

qreal Protractor::measuredAngle() const
{
    return std::abs(m_arm2 - m_arm1);
}

void Protractor::setArmAngles(qreal a1, qreal a2)
{
    m_arm1 = std::clamp(a1, 0.0, 180.0);
    m_arm2 = std::clamp(a2, 0.0, 180.0);
}

int Protractor::handleAt(const QPointF& local, qreal tol) const
{
    if (geom::distance(local, armKnob(m_arm2)) <= 20 + tol)
        return kArm2Handle;
    if (geom::distance(local, armKnob(m_arm1)) <= 20 + tol)
        return kArm1Handle;
    if (geom::distance(local, stampPos()) <= 18 + tol)
        return kStampHandle;
    return Instrument::handleAt(local, tol);
}

bool Protractor::beginInteraction(int handle, const QPointF& pagePos)
{
    return Instrument::beginInteraction(handle, pagePos);
}

void Protractor::updateInteraction(const QPointF& pagePos)
{
    if (m_handle != kArm1Handle && m_handle != kArm2Handle) {
        Instrument::updateInteraction(pagePos);
        return;
    }
    const QPointF local = toLocal(pagePos);
    qreal a = geom::radToDeg(std::atan2(-local.y(), local.x()));
    if (a < -90.0)
        a = 180.0;
    a = std::clamp(std::round(a), 0.0, 180.0);
    if (m_handle == kArm1Handle)
        m_arm1 = a;
    else
        m_arm2 = a;
}

InstrumentResult Protractor::endInteraction()
{
    InstrumentResult result;
    if (m_handle == kStampHandle) {
        const qreal reach = radius() * 0.8;
        auto armEnd = [&](qreal angle) {
            const qreal a = geom::degToRad(angle);
            return toPage(QPointF(std::cos(a) * reach, -std::sin(a) * reach));
        };
        QVector<QPointF> pts{armEnd(m_arm1), toPage(QPointF(0, 0)), armEnd(m_arm2)};
        result.objects.push_back(MeasurementObject::create(MeasureKind::Angle, pts, QColor(255, 213, 79)));
        result.text = QObject::tr("Angle stamped");
    }
    m_handle = kNoHandle;
    return result;
}

bool Protractor::edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const
{
    const QPointF local = toLocal(pagePos);
    const qreal R = radius() + 12;
    if (std::abs(local.y() - 26) <= tol && std::abs(local.x()) <= R + tol) {
        if (out) {
            out->kind = EdgeConstraint::Kind::Line;
            out->origin = toPage(QPointF(0, 26));
            out->direction = geom::rotated(QPointF(1, 0), m_rotation);
        }
        return true;
    }
    if (local.y() <= 4 && std::abs(geom::length(local) - R) <= tol) {
        if (out) {
            out->kind = EdgeConstraint::Kind::Circle;
            out->origin = toPage(QPointF(0, 0));
            out->radius = R;
        }
        return true;
    }
    return false;
}

QString Protractor::statusText() const
{
    return QStringLiteral("%1°").arg(geom::formatNumber(measuredAngle(), 0));
}

void Protractor::paint(QPainter& p, const InstrumentPaintContext& ctx) const
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal R = radius();
    p.setPen(cosmetic(kEdge, 1.2));
    p.setBrush(ctx.active ? kPlasticActive : kPlastic);
    p.drawPath(bodyShape());

    const bool fine = R * ctx.zoom * geom::kPi / 180.0 >= 3.0;
    for (int d = 0; d <= 180; ++d) {
        const bool ten = d % 10 == 0;
        const bool five = d % 5 == 0;
        if (!fine && !five)
            continue;
        const qreal len = ten ? 22 : (five ? 14 : 8);
        const qreal a = geom::degToRad(d);
        const QPointF dir(std::cos(a), -std::sin(a));
        p.setPen(cosmetic(kTick, ten ? 1.3 : 1.0));
        p.drawLine(dir * (R + 12), dir * (R + 12 - len));
        if (ten) {
            drawCenteredText(p, dir * (R - 26), QString::number(d), 14, kTick);
            drawCenteredText(p, dir * (R - 50), QString::number(180 - d), 11, QColor(60, 64, 62, 200));
        }
    }
    // Base line, inner arc and centre mark.
    p.setPen(cosmetic(kTick, 1.2));
    p.drawLine(QPointF(-R - 12, 0), QPointF(R + 12, 0));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(-R * 0.42, -R * 0.42, R * 0.84, R * 0.84), 0, 180 * 16);
    p.drawLine(QPointF(-10, 0), QPointF(10, 0));
    p.drawLine(QPointF(0, -10), QPointF(0, 10));

    // Measuring arms and angle arc.
    auto drawArm = [&](qreal angle, const QColor& c) {
        const QPointF knob = armKnob(angle);
        p.setPen(QPen(c, 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, 0), knob);
        p.setPen(cosmetic(QColor(255, 255, 255), 1.5));
        p.setBrush(c);
        p.drawEllipse(knob, 15, 15);
    };
    const qreal lo = std::min(m_arm1, m_arm2);
    const qreal span = measuredAngle();
    p.setBrush(QColor(255, 213, 79, 70));
    p.setPen(QPen(QColor(255, 193, 7), 2.0));
    p.drawPie(QRectF(-70, -70, 140, 140), static_cast<int>(lo * 16), static_cast<int>(span * 16));
    drawArm(m_arm1, kArm1);
    drawArm(m_arm2, kArm2);
    const qreal mid = geom::degToRad(lo + span / 2);
    drawCenteredText(p, QPointF(std::cos(mid), -std::sin(mid)) * 100.0,
                     QStringLiteral("%1°").arg(geom::formatNumber(span, 0)), 22, QColor(20, 20, 20), true);

    paintCloseButton(p, closeButtonPos(), 15);
    paintRotateKnob(p, rotateHandlePos(), 15);
    // Stamp button: records the measured angle on the page.
    p.setPen(cosmetic(kEdge, 1.0));
    p.setBrush(QColor(40, 44, 46, 230));
    p.drawEllipse(stampPos(), 15, 15);
    drawCenteredText(p, stampPos(), QStringLiteral("∠"), 18, QColor(255, 213, 79), true);
}

// ======================================================================================== SetSquare

void SetSquare::vertices(QPointF& a, QPointF& b, QPointF& c) const
{
    const qreal L = 9.0 * m_pxPerUnit;
    a = QPointF(0, 0);
    if (m_thirtySixty) {
        const qreal w = L * 1.25;
        b = QPointF(w, 0);
        c = QPointF(0, -w * std::tan(geom::degToRad(30.0)));
    } else {
        b = QPointF(L, 0);
        c = QPointF(0, -L);
    }
    const QPointF g = (a + b + c) / 3.0;
    a -= g;
    b -= g;
    c -= g;
}

QPainterPath SetSquare::bodyShape() const
{
    QPointF a, b, c;
    vertices(a, b, c);
    QPainterPath path;
    path.moveTo(a);
    path.lineTo(b);
    path.lineTo(c);
    path.closeSubpath();
    return path;
}

QPointF SetSquare::closeButtonPos() const
{
    QPointF a, b, c;
    vertices(a, b, c);
    return a + QPointF(30, -30);
}

QPointF SetSquare::rotateHandlePos() const
{
    QPointF a, b, c;
    vertices(a, b, c);
    return b + QPointF(-64, -16);
}

QPointF SetSquare::flipPos() const
{
    QPointF a, b, c;
    vertices(a, b, c);
    return c + QPointF(18, 76);
}

int SetSquare::handleAt(const QPointF& local, qreal tol) const
{
    if (geom::distance(local, flipPos()) <= 16 + tol)
        return kFlipHandle;
    return Instrument::handleAt(local, tol);
}

InstrumentResult SetSquare::endInteraction()
{
    if (m_handle == kFlipHandle)
        m_thirtySixty = !m_thirtySixty;
    m_handle = kNoHandle;
    return {};
}

bool SetSquare::edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const
{
    QPointF a, b, c;
    vertices(a, b, c);
    const QPointF local = toLocal(pagePos);
    const QPointF edges[3][2] = {{a, b}, {a, c}, {b, c}};
    qreal best = tol;
    bool found = false;
    for (const auto& e : edges) {
        const qreal d = geom::distanceToSegment(local, e[0], e[1]);
        if (d <= best) {
            best = d;
            found = true;
            if (out) {
                out->kind = EdgeConstraint::Kind::Line;
                out->origin = toPage(e[0]);
                out->direction = geom::rotated(geom::normalized(e[1] - e[0]), m_rotation);
            }
        }
    }
    return found;
}

QString SetSquare::statusText() const
{
    return QStringLiteral("%1°").arg(geom::formatNumber(lineAngle(m_rotation), 1));
}

void SetSquare::paint(QPainter& p, const InstrumentPaintContext& ctx) const
{
    p.setRenderHint(QPainter::Antialiasing, true);
    QPointF a, b, c;
    vertices(a, b, c);
    p.setPen(cosmetic(kEdge, 1.2));
    p.setBrush(ctx.active ? kPlasticActive : kPlastic);
    p.drawPath(bodyShape());

    // Inner cut-out.
    const QPointF g = (a + b + c) / 3.0;
    QPolygonF inner;
    for (const QPointF& v : {a, b, c})
        inner << g + (v - g) * 0.42;
    p.setBrush(QColor(0, 0, 0, 30));
    p.drawPolygon(inner);

    paintScale(p, a, geom::normalized(b - a), QPointF(0, -1), geom::distance(a, b) - 10, m_pxPerUnit, ctx.zoom, true);
    paintScale(p, a, geom::normalized(c - a), QPointF(1, 0), geom::distance(a, c) - 10, m_pxPerUnit, ctx.zoom, false);

    const QString angleB = m_thirtySixty ? QStringLiteral("30°") : QStringLiteral("45°");
    const QString angleC = m_thirtySixty ? QStringLiteral("60°") : QStringLiteral("45°");
    drawCenteredText(p, a + QPointF(58, -64), QStringLiteral("90°"), 15, kTick, true);
    drawCenteredText(p, b + QPointF(-120, -22), angleB, 15, kTick, true);
    drawCenteredText(p, c + QPointF(22, 130), angleC, 15, kTick, true);
    drawCenteredText(p, g + QPointF(0, 30), statusText(), 14, kTick);

    paintCloseButton(p, closeButtonPos(), 15);
    paintRotateKnob(p, rotateHandlePos(), 15);
    p.setPen(cosmetic(kEdge, 1.0));
    p.setBrush(QColor(40, 44, 46, 230));
    p.drawEllipse(flipPos(), 15, 15);
    drawCenteredText(p, flipPos(), m_thirtySixty ? QStringLiteral("45") : QStringLiteral("30"), 12,
                     QColor(240, 240, 240), true);
}

// ========================================================================================== Compass

void Compass::setRadius(qreal r)
{
    m_radius = std::clamp(r, 20.0, 5000.0);
}

QPointF Compass::hinge() const
{
    const qreal h = std::clamp(m_radius * 0.55, 70.0, 260.0);
    return QPointF(m_radius / 2, -h);
}

QPainterPath Compass::bodyShape() const
{
    QPainterPath legs;
    legs.moveTo(0, 0);
    legs.lineTo(hinge());
    legs.lineTo(m_radius, 0);
    QPainterPathStroker stroker;
    stroker.setWidth(26);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath shape = stroker.createStroke(legs);
    shape.addEllipse(hinge(), 22, 22);
    shape.setFillRule(Qt::WindingFill);
    return shape;
}

QPointF Compass::closeButtonPos() const
{
    return QPointF(-40, hinge().y() - 10);
}

QPointF Compass::rotateHandlePos() const
{
    return QPointF(m_radius * 0.25 - 36, hinge().y() * 0.5);
}

int Compass::handleAt(const QPointF& local, qreal tol) const
{
    if (geom::distance(local, QPointF(m_radius, 0)) <= 26 + tol)
        return kPencilHandle;
    if (geom::distance(local, hinge()) <= 24 + tol)
        return kRadiusHandle;
    return Instrument::handleAt(local, tol);
}

bool Compass::beginInteraction(int handle, const QPointF& pagePos)
{
    if (!Instrument::beginInteraction(handle, pagePos))
        return false;
    if (handle == kPencilHandle) {
        m_arc.clear();
        StrokePoint sp;
        sp.pos = toPage(QPointF(m_radius, 0));
        m_arc.push_back(sp);
        m_sweep = 0.0;
        m_startAngle = m_rotation;
        m_lastAngle = m_rotation;
    }
    return true;
}

void Compass::appendArcTo(qreal target)
{
    qreal current = m_startAngle + m_sweep;
    const qreal step = std::min(2.0, std::max(0.2, geom::radToDeg(3.0 / m_radius)));
    const int n = static_cast<int>(std::ceil(std::abs(target - current) / step));
    for (int i = 1; i <= n; ++i) {
        const qreal a = current + (target - current) * i / n;
        StrokePoint sp;
        sp.pos = m_position + geom::rotated(QPointF(m_radius, 0), a);
        m_arc.push_back(sp);
    }
    m_sweep = target - m_startAngle;
}

void Compass::updateInteraction(const QPointF& pagePos)
{
    switch (m_handle) {
    case kPencilHandle: {
        const qreal a = geom::angleDeg(pagePos - m_position);
        qreal d = geom::angleDifference(m_lastAngle, a);
        if (m_sweep + d > 360.0)
            d = 360.0 - m_sweep;
        else if (m_sweep + d < -360.0)
            d = -360.0 - m_sweep;
        appendArcTo(m_startAngle + m_sweep + d);
        m_lastAngle = geom::normalizeDegrees(m_lastAngle + d);
        setRotation(m_lastAngle);
        break;
    }
    case kRadiusHandle: {
        const QPointF local = toLocal(pagePos);
        const qreal mm = m_pxPerUnit / 10.0;
        setRadius(std::round(std::max(20.0, 2.0 * local.x()) / mm) * mm);
        break;
    }
    default:
        Instrument::updateInteraction(pagePos);
        break;
    }
}

InstrumentResult Compass::endInteraction()
{
    InstrumentResult result;
    if (m_handle == kPencilHandle && std::abs(m_sweep) >= 1.0 && m_arc.size() >= 2) {
        InkStyle ink = m_ink;
        ink.pressure = false;
        if (ink.style == StrokeStyle::Highlighter)
            ink.style = StrokeStyle::Pen;
        result.objects.push_back(StrokeObject::fromPagePoints(m_arc, ink));
        result.text = QObject::tr("Arc drawn");
    }
    m_arc.clear();
    m_sweep = 0.0;
    m_handle = kNoHandle;
    return result;
}

void Compass::cancelInteraction()
{
    m_arc.clear();
    m_sweep = 0.0;
    Instrument::cancelInteraction();
}

QString Compass::statusText() const
{
    QString s = QStringLiteral("r = ") + unitText(m_radius, m_pxPerUnit, m_unitLabel);
    if (m_handle == kPencilHandle)
        s += QStringLiteral(" · %1°").arg(geom::formatNumber(std::abs(m_sweep), 0));
    return s;
}

void Compass::paint(QPainter& p, const InstrumentPaintContext& ctx) const
{
    Q_UNUSED(ctx);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Pending arc (page coordinates mapped into the local frame).
    if (m_arc.size() >= 2) {
        QVector<StrokePoint> local = m_arc;
        for (StrokePoint& sp : local)
            sp.pos = toLocal(sp.pos);
        InkStyle ink = m_ink;
        ink.pressure = false;
        StrokeObject::paintPoints(p, local, ink);
    }

    const QPointF h = hinge();
    const QPointF pencil(m_radius, 0);
    // Radius guide.
    QPen guide(QColor(255, 213, 79, 200), 1.5, Qt::DashLine);
    guide.setCosmetic(true);
    p.setPen(guide);
    p.drawLine(QPointF(0, 0), pencil);
    drawCenteredText(p, QPointF(m_radius / 2, 18), QStringLiteral("r = ") + unitText(m_radius, m_pxPerUnit, m_unitLabel),
                     16, QColor(255, 224, 130), true);

    // Legs.
    const QColor metal(120, 128, 134, 235);
    p.setPen(QPen(QColor(30, 34, 36, 220), 16, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(0, -6), h);
    p.drawLine(h, pencil + QPointF(0, -10));
    p.setPen(QPen(metal, 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(0, -6), h);
    p.drawLine(h, pencil + QPointF(0, -10));

    // Needle.
    p.setPen(QPen(QColor(220, 220, 220), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(0, -8), QPointF(0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(239, 83, 80));
    p.drawEllipse(QPointF(0, 0), 5, 5);

    // Pencil tip in the current ink colour.
    QPolygonF tip;
    tip << pencil + QPointF(-8, -14) << pencil + QPointF(8, -14) << pencil;
    p.setBrush(QColor(250, 224, 170));
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.drawPolygon(tip);
    p.setBrush(m_ink.color);
    p.drawEllipse(pencil, 4, 4);
    // Pencil grip ring (drag to draw).
    p.setPen(QPen(QColor(255, 255, 255, 180), 2, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(pencil, 24, 24);

    // Hinge (drag to change radius).
    p.setPen(QPen(QColor(20, 20, 20), 1.5));
    p.setBrush(QColor(60, 66, 70));
    p.drawEllipse(h, 18, 18);
    p.setPen(QPen(QColor(230, 230, 230), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(h + QPointF(-8, 0), h + QPointF(8, 0));
    p.drawLine(h + QPointF(-8, 0), h + QPointF(-4, -4));
    p.drawLine(h + QPointF(-8, 0), h + QPointF(-4, 4));
    p.drawLine(h + QPointF(8, 0), h + QPointF(4, -4));
    p.drawLine(h + QPointF(8, 0), h + QPointF(4, 4));

    paintCloseButton(p, closeButtonPos(), 15);
    paintRotateKnob(p, rotateHandlePos(), 15);
}

} // namespace cb
