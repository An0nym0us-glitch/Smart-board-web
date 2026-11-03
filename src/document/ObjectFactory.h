#pragma once

#include "document/DocumentObject.h"

#include <QSize>

namespace cb {

/// Creates document objects by type, used by deserialisation, the clipboard and importers.
class ObjectFactory
{
public:
    static ObjectPtr create(ObjectType type);
    /// Parses an object from JSON. Returns nullptr for unknown types or malformed data.
    static ObjectPtr fromJson(const QJsonObject& obj);

    /// Image object for an asset, scaled to a comfortable size on the page.
    static ObjectPtr createImage(const QString& key, const QSize& pixelSize, const QPointF& center);
    /// Text object with the default classroom format.
    static ObjectPtr createText(const QString& text, const QPointF& center);
};

} // namespace cb
