#pragma once

#include "document/Document.h"

#include <functional>

namespace cb {

/// Progress callback: (done, total). Return false to cancel.
using ExportProgress = std::function<bool(int done, int total)>;

/// Vector PDF export: text stays text, shapes and ink stay vector, images are embedded once per
/// page at full quality. One PDF page per lesson page with the page's own logical size (a 16:9
/// board page, A4, A3 ...). Pages are rendered from document coordinates, never from the view, so
/// zoom and scrolling have no influence.
class PdfExporter
{
public:
    static bool exportPages(const DocumentSnapshot& snapshot, const QString& path, QString* error,
                            const ExportProgress& progress = {});
};

/// PowerPoint (.pptx) export. Each page becomes a slide with a high resolution rendering of the
/// complete page (native conversion of ink/formulas is not possible in PowerPoint, so visual
/// fidelity is preserved instead). The slide size follows the aspect ratio of the first page;
/// pages with another aspect ratio are fitted whole, never cropped or stretched.
class PptxExporter
{
public:
    struct Slide
    {
        QByteArray png;
        QSize pixelSize;
        QColor background;
    };
    static QByteArray buildPackage(const QVector<Slide>& slides, const QString& title);
    /// Slide size in EMU (width 13.333 in, height from the first page's aspect ratio).
    static QSize slideSize(const QVector<Slide>& slides);
    /// Picture placement (EMU) of a page on the slide: fitted whole and centred.
    static QRect pictureRect(const QSize& slide, const QSize& pagePixels);
    static bool exportPages(const DocumentSnapshot& snapshot, const QString& path, QString* error,
                            const ExportProgress& progress = {});
    /// Builds the package in memory (used by tests).
    static QByteArray buildPackage(const QVector<QByteArray>& slidePngs, const QString& title);
};

/// PNG export of single pages.
class ImageExporter
{
public:
    static QImage renderPage(const Page& page, const ImageStore& images, const CoordinateSystem& coordinates,
                             int pixelWidth);
    static bool exportPage(const DocumentSnapshot& snapshot, int index, const QString& path, QString* error);
};

} // namespace cb
