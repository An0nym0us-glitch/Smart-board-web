#pragma once

#include "document/DocumentObject.h"

#include <QPainterPath>
#include <QPointF>
#include <QTransform>

#include <vector>

class QPainter;

namespace cb {

class CoordinateSystem;

/// A straight edge or circular arc that ink can be drawn along.
struct EdgeConstraint
{
    enum class Kind { None, Line, Circle };
    Kind kind = Kind::None;
    QPointF origin;    ///< point on the line, or circle centre
    QPointF direction; ///< unit direction for lines
    qreal radius = 0.0;

    bool isValid() const { return kind != Kind::None; }
    QPointF project(const QPointF& p) const;
    qreal distanceTo(const QPointF& p) const;
};

struct InstrumentPaintContext
{
    qreal zoom = 1.0;
    const CoordinateSystem* coordinates = nullptr;
    bool active = false; ///< being manipulated
};

/// Result of an instrument interaction: objects to add to the page (compass arcs, stamps).
struct InstrumentResult
{
    std::vector<ObjectPtr> objects;
    QString text;
};

/// A physical drawing instrument lying on the board (ruler, protractor, set square, compass).
///
/// Instruments are not document content: they are overlays in page coordinates that can be moved,
/// rotated and used to construct content. Handles are identified by integers: -1 none, 0 body
/// (move), others instrument specific.
class Instrument
{
public:
    enum class Kind { Ruler, Protractor, SetSquare, Compass };
    static constexpr int kNoHandle = -1;
    static constexpr int kBodyHandle = 0;
    static constexpr int kCloseHandle = 1;
    static constexpr int kRotateHandle = 2;

    virtual ~Instrument() = default;
    virtual Kind kind() const = 0;

    QPointF position() const { return m_position; }
    void setPosition(const QPointF& p) { m_position = p; }
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal deg);
    QTransform transform() const;
    QPointF toLocal(const QPointF& page) const;
    QPointF toPage(const QPointF& local) const { return transform().map(local); }

    /// Body outline in local coordinates.
    virtual QPainterPath bodyShape() const = 0;
    /// Page-space rect covering body and handles.
    QRectF sceneBounds() const;

    virtual void paint(QPainter& painter, const InstrumentPaintContext& ctx) const = 0;

    /// Handle under a local position; tol is in page units.
    virtual int handleAt(const QPointF& local, qreal tol) const;

    /// Pointer interaction. Returns false to decline.
    virtual bool beginInteraction(int handle, const QPointF& pagePos);
    virtual void updateInteraction(const QPointF& pagePos);
    virtual InstrumentResult endInteraction();
    virtual void cancelInteraction();
    bool isInteracting() const { return m_handle != kNoHandle; }
    int activeHandle() const { return m_handle; }

    /// Constraint for ink starting near an edge.
    virtual bool edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const;

    /// Two-finger manipulation: translate and rotate around a page point.
    void applyGesture(const QPointF& pageCenter, const QPointF& pageDelta, qreal rotationDelta);

    /// Live read-out shown while manipulating (angle, length ...).
    virtual QString statusText() const { return QString(); }

    /// Units per page pixel from the document coordinate system.
    void setPixelsPerUnit(qreal ppu) { m_pxPerUnit = ppu > 1 ? ppu : 40.0; }
    qreal pixelsPerUnit() const { return m_pxPerUnit; }
    void setUnitLabel(const QString& label) { m_unitLabel = label; }

protected:
    /// Draws the round close (x) button centred at a local point.
    static void paintCloseButton(QPainter& p, const QPointF& center, qreal radius);
    static void paintRotateKnob(QPainter& p, const QPointF& center, qreal radius);
    virtual QPointF closeButtonPos() const = 0;
    virtual QPointF rotateHandlePos() const = 0;
    qreal handleRadius() const { return 22.0; }

    QPointF m_position;
    qreal m_rotation = 0.0;
    qreal m_pxPerUnit = 40.0;
    QString m_unitLabel = QStringLiteral("cm");

    int m_handle = kNoHandle;
    QPointF m_grabLocal;       ///< where the body was grabbed (local)
    QPointF m_grabPage;
    qreal m_grabAngleOffset = 0.0;
};

} // namespace cb
