#pragma once

#include "core/Id.h"

#include <QColor>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QTransform>
#include <QVector>

#include <memory>

class QPainter;

namespace cb {

class ImageStore;
class CoordinateSystem;
class Page;

enum class ObjectType {
    Stroke,
    Shape,
    Text,
    Equation,
    Image,
    Graph,
    Geometry,
    Measurement,
    Table,
};

QString objectTypeName(ObjectType type);
bool objectTypeFromName(const QString& name, ObjectType* type);

/// Everything an object needs to render itself.
struct RenderContext
{
    qreal zoom = 1.0;                                   ///< device independent pixels per page unit
    const ImageStore* images = nullptr;                 ///< shared image assets
    const CoordinateSystem* coordinates = nullptr;      ///< page-global math coordinate system
    const Page* page = nullptr;                         ///< page being rendered (for coordinate lookups)
    bool exporting = false;                             ///< true for PDF/PPTX/PNG output
};

/// Which handle initiated a resize, so objects can interpret it (e.g. text wraps on side handles).
enum class ResizeHint { Corner, Horizontal, Vertical };

/// Base class of every item stored on a page.
///
/// Objects keep their geometry in a local frame centred on the origin. The object transform
/// (translation + rotation) maps local coordinates to page coordinates. All mutations of objects
/// that live in a document must happen through document commands so that undo/redo works.
class DocumentObject
{
public:
    explicit DocumentObject(ObjectType type);
    virtual ~DocumentObject();

    ObjectType type() const { return m_type; }

    const ObjectId& id() const { return m_id; }
    void setId(const ObjectId& id) { m_id = id; }

    QPointF position() const { return m_position; }
    void setPosition(const QPointF& pos);

    qreal rotation() const { return m_rotation; }
    void setRotation(qreal degrees);

    /// Local -> page transform.
    QTransform transform() const;
    QPointF mapToPage(const QPointF& local) const { return transform().map(local); }
    QPointF mapFromPage(const QPointF& page) const;

    /// Content bounds in local coordinates (excluding stroke outlines).
    virtual QRectF localBounds() const = 0;

    /// Extra margin around localBounds covered by painting (pen width, labels ...).
    virtual qreal outlineMargin() const { return 0.0; }

    /// Page-space bounds including the outline margin. Cached.
    QRectF sceneBounds() const;

    /// Paints the object. The painter is already transformed into the object's local frame.
    virtual void paint(QPainter& painter, const RenderContext& ctx) const = 0;

    /// Hit test in page coordinates with a tolerance in page units.
    bool hitTest(const QPointF& pagePos, qreal tolerance) const;

    /// True if pagePos lies inside a closed outline even where nothing is painted (e.g. inside an
    /// unfilled rectangle). Used by selection as a fallback when no painted object is hit.
    virtual bool enclosesPoint(const QPointF& pagePos) const { Q_UNUSED(pagePos); return false; }

    /// Returns true if the object is (mostly) inside the page-space polygon.
    virtual bool isInsidePolygon(const QPolygonF& pagePolygon) const;

    virtual bool canResize() const { return true; }
    virtual bool canRotate() const { return true; }
    virtual bool keepAspectRatio() const { return false; }

    /// Resizes so that the content occupies newLocalRect (expressed in the current local frame).
    void resizeTo(const QRectF& newLocalRect, ResizeHint hint = ResizeHint::Corner);

    /// Editable points in local coordinates (line end points, polygon vertices ...).
    virtual QVector<QPointF> controlPoints() const { return {}; }
    /// Moves a control point to a page position.
    void moveControlPointTo(int index, const QPointF& pagePos);

    /// Applies a colour to the object. Returns false if not supported.
    virtual bool setColor(const QColor& color);
    virtual QColor color() const { return QColor(); }

    /// Collects image asset keys referenced by the object.
    virtual void collectImageKeys(QSet<QString>& keys) const { Q_UNUSED(keys); }

    /// Objects that define their own mathematical coordinate system (graphs) return it here.
    virtual const CoordinateSystem* mathCoordinateSystem() const { return nullptr; }

    /// True if the object supports an edit action (text, equations, graphs, tables).
    virtual bool isEditable() const { return false; }

    virtual std::unique_ptr<DocumentObject> clone() const = 0;

    QJsonObject toJson() const;
    /// Restores all state from JSON. Returns false on malformed data.
    bool fromJson(const QJsonObject& obj);

protected:
    DocumentObject(const DocumentObject& other) = default;
    DocumentObject& operator=(const DocumentObject& other) = delete;

    /// Hit test in local coordinates. Default: inside local bounds (+tolerance).
    virtual bool hitTestLocal(const QPointF& local, qreal tolerance) const;

    /// Scales content (centred on the local origin) to the new size.
    virtual void applyResize(const QSizeF& newSize, ResizeHint hint) = 0;

    /// Moves a control point in local coordinates. Implementations should call recenter() after.
    virtual void setControlPoint(int index, const QPointF& local) { Q_UNUSED(index); Q_UNUSED(local); }

    /// For point-set objects: shift content by delta in local coordinates.
    virtual void translateContent(const QPointF& delta) { Q_UNUSED(delta); }

    /// Recentres content around the local origin, compensating the position.
    void recenter();

    virtual void writeProperties(QJsonObject& obj) const = 0;
    virtual bool readProperties(const QJsonObject& obj) = 0;

    void invalidateBounds() const { m_boundsValid = false; }

private:
    ObjectType m_type;
    ObjectId m_id;
    QPointF m_position;
    qreal m_rotation = 0.0;
    mutable QRectF m_sceneBounds;
    mutable bool m_boundsValid = false;
};

using ObjectPtr = std::unique_ptr<DocumentObject>;

} // namespace cb
