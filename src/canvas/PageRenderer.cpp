#include "canvas/PageRenderer.h"

#include "document/CoordinateResolver.h"
#include "document/DocumentObject.h"
#include "document/ImageStore.h"
#include "document/Page.h"
#include "math/CoordinateSystem.h"

#include <QPainter>

namespace cb {

void PageRenderer::render(QPainter& painter, const Page& page, const QRectF& pageArea, qreal zoom,
                          const ImageStore& images, const CoordinateSystem& coordinates, const Options& options)
{
    const CoordinateSystem pageCoordinates = pageCoordinateSystem(coordinates, &page);
    if (options.drawBackground)
        TemplateRenderer::paint(painter, page.background(), pageArea, zoom, &images, pageCoordinates, page.frameRect());

    RenderContext ctx;
    ctx.zoom = zoom;
    ctx.images = &images;
    ctx.coordinates = &pageCoordinates;
    ctx.page = &page;
    ctx.exporting = options.exporting;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (const auto& object : page.objects()) {
        if (options.hidden && options.hidden->contains(object->id()))
            continue;
        if (!object->sceneBounds().intersects(pageArea))
            continue;
        painter.save();
        painter.setTransform(object->transform(), true);
        object->paint(painter, ctx);
        painter.restore();
    }
}

QImage PageRenderer::renderToImage(const Page& page, const QRectF& pageArea, const QSize& pixelSize,
                                   const ImageStore& images, const CoordinateSystem& coordinates, bool exporting)
{
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(page.background().background);
    if (pageArea.isEmpty() || pixelSize.isEmpty())
        return image;
    QPainter painter(&image);
    const qreal sx = pixelSize.width() / pageArea.width();
    const qreal sy = pixelSize.height() / pageArea.height();
    painter.scale(sx, sy);
    painter.translate(-pageArea.topLeft());
    Options options;
    options.exporting = exporting;
    render(painter, page, pageArea, std::min(sx, sy), images, coordinates, options);
    painter.end();
    return image;
}

} // namespace cb
