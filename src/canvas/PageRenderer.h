#pragma once

#include <QImage>
#include <QRectF>
#include <QSet>
#include <QSize>

#include "core/Id.h"

class QPainter;

namespace cb {

class Page;
class ImageStore;
class CoordinateSystem;

/// Renders pages. Shared by the canvas, thumbnails and all exporters so output is identical.
class PageRenderer
{
public:
    struct Options
    {
        bool drawBackground = true;
        bool exporting = false;
        /// Objects to skip (e.g. text currently being edited inline).
        const QSet<ObjectId>* hidden = nullptr;
    };

    /// Renders the part of the page given by pageArea. The painter must already map page
    /// coordinates to device coordinates; zoom is the resulting scale (for level of detail).
    static void render(QPainter& painter, const Page& page, const QRectF& pageArea, qreal zoom,
                       const ImageStore& images, const CoordinateSystem& coordinates, const Options& options);

    /// Renders pageArea of a page into an image of the given pixel size.
    static QImage renderToImage(const Page& page, const QRectF& pageArea, const QSize& pixelSize,
                                const ImageStore& images, const CoordinateSystem& coordinates,
                                bool exporting = true);
};

} // namespace cb
