#pragma once

#include "document/StrokeObject.h"
#include "geometry/Instrument.h"

#include <QVector>

namespace cb {

std::unique_ptr<Instrument> createInstrument(Instrument::Kind kind);

/// Straight ruler with a unit scale. Ink started near a long edge is drawn along it.
class Ruler final : public Instrument
{
public:
    static constexpr int kLengthHandle = 3;

    Kind kind() const override { return Kind::Ruler; }
    QPainterPath bodyShape() const override;
    void paint(QPainter& painter, const InstrumentPaintContext& ctx) const override;
    int handleAt(const QPointF& local, qreal tol) const override;
    bool beginInteraction(int handle, const QPointF& pagePos) override;
    void updateInteraction(const QPointF& pagePos) override;
    bool edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const override;
    QString statusText() const override;

    qreal lengthUnits() const { return m_units; }
    void setLengthUnits(qreal units);

protected:
    QPointF closeButtonPos() const override;
    QPointF rotateHandlePos() const override;

private:
    qreal length() const { return m_units * m_pxPerUnit + 2 * kMargin; }
    static constexpr qreal kHeight = 96.0;
    static constexpr qreal kMargin = 18.0;
    qreal m_units = 30.0;
};

/// Semicircular protractor with two measuring arms.
class Protractor final : public Instrument
{
public:
    static constexpr int kArm1Handle = 3;
    static constexpr int kArm2Handle = 4;
    static constexpr int kStampHandle = 5;

    Kind kind() const override { return Kind::Protractor; }
    QPainterPath bodyShape() const override;
    void paint(QPainter& painter, const InstrumentPaintContext& ctx) const override;
    int handleAt(const QPointF& local, qreal tol) const override;
    bool beginInteraction(int handle, const QPointF& pagePos) override;
    void updateInteraction(const QPointF& pagePos) override;
    InstrumentResult endInteraction() override;
    bool edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const override;
    QString statusText() const override;

    qreal measuredAngle() const;
    void setArmAngles(qreal a1, qreal a2);
    qreal arm1() const { return m_arm1; }
    qreal arm2() const { return m_arm2; }

protected:
    QPointF closeButtonPos() const override;
    QPointF rotateHandlePos() const override;

private:
    qreal radius() const { return 7.0 * m_pxPerUnit; }
    QPointF armKnob(qreal angle) const;
    QPointF stampPos() const;
    qreal m_arm1 = 0.0;
    qreal m_arm2 = 60.0;
};

/// Set square (45-45-90 or 30-60-90) with ruled legs.
class SetSquare final : public Instrument
{
public:
    static constexpr int kFlipHandle = 3;

    Kind kind() const override { return Kind::SetSquare; }
    QPainterPath bodyShape() const override;
    void paint(QPainter& painter, const InstrumentPaintContext& ctx) const override;
    int handleAt(const QPointF& local, qreal tol) const override;
    InstrumentResult endInteraction() override;
    bool edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const override;
    QString statusText() const override;

    bool isThirtySixty() const { return m_thirtySixty; }
    void setThirtySixty(bool on) { m_thirtySixty = on; }

protected:
    QPointF closeButtonPos() const override;
    QPointF rotateHandlePos() const override;

private:
    /// Vertices: right angle, end of the horizontal leg, end of the vertical leg (local frame).
    void vertices(QPointF& a, QPointF& b, QPointF& c) const;
    QPointF flipPos() const;
    bool m_thirtySixty = false;
};

/// Drawing compass: needle at the position, pencil at distance radius along the rotation.
class Compass final : public Instrument
{
public:
    static constexpr int kPencilHandle = 3;
    static constexpr int kRadiusHandle = 4;

    Kind kind() const override { return Kind::Compass; }
    QPainterPath bodyShape() const override;
    void paint(QPainter& painter, const InstrumentPaintContext& ctx) const override;
    int handleAt(const QPointF& local, qreal tol) const override;
    bool beginInteraction(int handle, const QPointF& pagePos) override;
    void updateInteraction(const QPointF& pagePos) override;
    InstrumentResult endInteraction() override;
    void cancelInteraction() override;
    QString statusText() const override;

    qreal radius() const { return m_radius; }
    void setRadius(qreal r);
    void setInk(const InkStyle& ink) { m_ink = ink; }
    const QVector<StrokePoint>& pendingArc() const { return m_arc; }
    qreal sweep() const { return m_sweep; }

protected:
    QPointF closeButtonPos() const override;
    QPointF rotateHandlePos() const override;

private:
    QPointF hinge() const;
    void appendArcTo(qreal angle);
    qreal m_radius = 160.0;
    bool m_radiusInitialised = false;
    InkStyle m_ink;
    QVector<StrokePoint> m_arc; // page coordinates
    qreal m_sweep = 0.0;
    qreal m_startAngle = 0.0;
    qreal m_lastAngle = 0.0;
};

} // namespace cb
