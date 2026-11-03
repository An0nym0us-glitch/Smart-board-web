#include "document/ObjectFactory.h"

#include "document/ImageObject.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "graph/GraphObject.h"
#include "graph/TableObject.h"
#include "math/equation/EquationObject.h"

#include <algorithm>

namespace cb {

ObjectPtr ObjectFactory::create(ObjectType type)
{
    switch (type) {
    case ObjectType::Stroke: return std::make_unique<StrokeObject>();
    case ObjectType::Shape: return std::make_unique<ShapeObject>();
    case ObjectType::Text: return std::make_unique<TextObject>();
    case ObjectType::Image: return std::make_unique<ImageObject>();
    case ObjectType::Measurement: return std::make_unique<MeasurementObject>();
    case ObjectType::Geometry: return std::make_unique<GeometryObject>();
    case ObjectType::Equation: return std::make_unique<EquationObject>();
    case ObjectType::Graph: return std::make_unique<GraphObject>();
    case ObjectType::Table: return std::make_unique<TableObject>();
    }
    return nullptr;
}

ObjectPtr ObjectFactory::fromJson(const QJsonObject& obj)
{
    ObjectType type;
    if (!objectTypeFromName(obj.value(QStringLiteral("type")).toString(), &type))
        return nullptr;
    ObjectPtr o = create(type);
    if (!o || !o->fromJson(obj))
        return nullptr;
    return o;
}

ObjectPtr ObjectFactory::createImage(const QString& key, const QSize& pixelSize, const QPointF& center)
{
    if (key.isEmpty() || pixelSize.isEmpty())
        return nullptr;
    QSizeF size(pixelSize);
    const QSizeF maxSize(900, 640);
    if (size.width() > maxSize.width() || size.height() > maxSize.height())
        size.scale(maxSize, Qt::KeepAspectRatio);
    return ImageObject::create(key, size, center);
}

ObjectPtr ObjectFactory::createText(const QString& text, const QPointF& center)
{
    TextFormat format;
    const qreal width = std::max(120.0, TextObject::naturalWidth(text, format) + 8.0);
    auto t = TextObject::create(text, QPointF(), format, width);
    t->setPosition(center);
    return t;
}

} // namespace cb
