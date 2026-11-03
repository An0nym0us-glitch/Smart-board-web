#include "export/Exporters.h"

#include "canvas/PageRenderer.h"
#include "document/PageSize.h"
#include "export/ZipWriter.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QImageWriter>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>

namespace cb {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("Export", s); }

// Default 16:9 slide width (13.333 inch). Slide heights follow the page aspect ratio.
constexpr qint64 kSlideCx = 12192000; // EMU
constexpr qint64 kEmuPerMm = 36000;

/// Physical size of an exported page: document units are 40 per cm, so an A4 page becomes an A4
/// PDF page and a 16:9 board page a 48 × 27 cm page.
QPageSize pageSizeFor(const QRectF& area)
{
    const QSizeF mm(area.width() / pagesize::kUnitsPerCm * 10.0, area.height() / pagesize::kUnitsPerCm * 10.0);
    return QPageSize(mm, QPageSize::Millimeter, QStringLiteral("ClassBoard page"), QPageSize::ExactMatch);
}

QString xmlEscape(const QString& s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

const char* kNs = "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                  "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
                  "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"";

const char* kXmlHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

const char* kEmptyGroup = "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
                          "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
                          "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>";

QByteArray themeXml()
{
    return QByteArray(kXmlHeader) + R"(<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="ClassBoard">
<a:themeElements>
<a:clrScheme name="ClassBoard">
<a:dk1><a:srgbClr val="000000"/></a:dk1><a:lt1><a:srgbClr val="FFFFFF"/></a:lt1>
<a:dk2><a:srgbClr val="1F2B26"/></a:dk2><a:lt2><a:srgbClr val="E9EEEB"/></a:lt2>
<a:accent1><a:srgbClr val="5FD3A5"/></a:accent1><a:accent2><a:srgbClr val="4FB3FF"/></a:accent2>
<a:accent3><a:srgbClr val="FFD54F"/></a:accent3><a:accent4><a:srgbClr val="EF5350"/></a:accent4>
<a:accent5><a:srgbClr val="BA68C8"/></a:accent5><a:accent6><a:srgbClr val="81C784"/></a:accent6>
<a:hlink><a:srgbClr val="4FB3FF"/></a:hlink><a:folHlink><a:srgbClr val="BA68C8"/></a:folHlink>
</a:clrScheme>
<a:fontScheme name="ClassBoard">
<a:majorFont><a:latin typeface="Segoe UI"/><a:ea typeface=""/><a:cs typeface=""/></a:majorFont>
<a:minorFont><a:latin typeface="Segoe UI"/><a:ea typeface=""/><a:cs typeface=""/></a:minorFont>
</a:fontScheme>
<a:fmtScheme name="ClassBoard">
<a:fillStyleLst>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
</a:fillStyleLst>
<a:lnStyleLst>
<a:ln w="9525"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
<a:ln w="25400"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
<a:ln w="38100"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
</a:lnStyleLst>
<a:effectStyleLst>
<a:effectStyle><a:effectLst/></a:effectStyle>
<a:effectStyle><a:effectLst/></a:effectStyle>
<a:effectStyle><a:effectLst/></a:effectStyle>
</a:effectStyleLst>
<a:bgFillStyleLst>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
</a:bgFillStyleLst>
</a:fmtScheme>
</a:themeElements>
<a:objectDefaults/>
<a:extraClrSchemeLst/>
</a:theme>)";
}

QByteArray rels(const QVector<QPair<QString, QString>>& items)
{
    // items: (type suffix URL, target); ids are rId1..n in order.
    QByteArray out(kXmlHeader);
    out += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    int id = 1;
    for (const auto& item : items) {
        out += QStringLiteral("<Relationship Id=\"rId%1\" Type=\"%2\" Target=\"%3\"/>")
                   .arg(id++)
                   .arg(item.first, item.second)
                   .toUtf8();
    }
    out += "</Relationships>";
    return out;
}

const QString kRelBase = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");
} // namespace

// ============================================================================================ PDF

bool PdfExporter::exportPages(const DocumentSnapshot& snapshot, const QString& path, QString* error,
                              const ExportProgress& progress)
{
    if (snapshot.pages.empty()) {
        if (error)
            *error = tr("There are no pages to export.");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    {
        QPdfWriter writer(&file);
        writer.setCreator(QStringLiteral("ClassBoard"));
        writer.setTitle(snapshot.metadata.title.isEmpty() ? QStringLiteral("ClassBoard lesson") : snapshot.metadata.title);
        writer.setPageSize(pageSizeFor(snapshot.pages.front()->exportRect()));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        writer.setResolution(144);
        QPainter painter;
        if (!painter.begin(&writer)) {
            if (error)
                *error = tr("Could not start the PDF writer.");
            file.cancelWriting();
            return false;
        }
        const int total = static_cast<int>(snapshot.pages.size());
        for (int i = 0; i < total; ++i) {
            const Page& page = *snapshot.pages[static_cast<size_t>(i)];
            // The complete logical page, rendered from document coordinates (never the view).
            const QRectF area = page.exportRect();
            if (i > 0) {
                writer.setPageSize(pageSizeFor(area));
                writer.newPage();
            }
            const qreal s = std::min(writer.width() / area.width(), writer.height() / area.height());
            painter.save();
            painter.scale(s, s);
            painter.translate(-area.topLeft());
            PageRenderer::Options options;
            options.exporting = true;
            PageRenderer::render(painter, page, area, s, snapshot.images, snapshot.coordinates, options);
            painter.restore();
            if (progress && !progress(i + 1, total)) {
                painter.end();
                file.cancelWriting();
                if (error)
                    *error = tr("Export cancelled.");
                return false;
            }
        }
        painter.end();
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

// =========================================================================================== PPTX

QByteArray PptxExporter::buildPackage(const QVector<QByteArray>& slidePngs, const QString& title)
{
    QVector<Slide> slides;
    for (const QByteArray& png : slidePngs)
        slides.push_back({png, QSize(16, 9), QColor(Qt::white)});
    return buildPackage(slides, title);
}

QByteArray PptxExporter::buildPackage(const QVector<Slide>& slides, const QString& title)
{
    ZipWriter zip;
    const int n = slides.size();
    // Slide size follows the first page; pages with another aspect ratio are fitted whole.
    const QSize first = n > 0 && !slides.front().pixelSize.isEmpty() ? slides.front().pixelSize : QSize(16, 9);
    const QSize slideEmu = slideSize(slides);
    const qint64 slideCx = slideEmu.width();
    const qint64 slideCy = slideEmu.height();

    // [Content_Types].xml
    QByteArray types(kXmlHeader);
    types += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
             "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
             "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
             "<Default Extension=\"png\" ContentType=\"image/png\"/>"
             "<Override PartName=\"/ppt/presentation.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml\"/>"
             "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml\"/>"
             "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml\"/>"
             "<Override PartName=\"/ppt/theme/theme1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>"
             "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
             "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>";
    for (int i = 1; i <= n; ++i)
        types += QStringLiteral("<Override PartName=\"/ppt/slides/slide%1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>")
                     .arg(i)
                     .toUtf8();
    types += "</Types>";
    zip.addFile(QStringLiteral("[Content_Types].xml"), types);

    zip.addFile(QStringLiteral("_rels/.rels"),
                rels({{kRelBase + QStringLiteral("officeDocument"), QStringLiteral("ppt/presentation.xml")},
                      {QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"),
                       QStringLiteral("docProps/core.xml")},
                      {kRelBase + QStringLiteral("extended-properties"), QStringLiteral("docProps/app.xml")}}));

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QByteArray core(kXmlHeader);
    core += QStringLiteral("<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
                           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                           "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                           "<dc:title>%1</dc:title><dc:creator>ClassBoard</dc:creator>"
                           "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%2</dcterms:created>"
                           "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%2</dcterms:modified>"
                           "</cp:coreProperties>")
                .arg(xmlEscape(title), now)
                .toUtf8();
    zip.addFile(QStringLiteral("docProps/core.xml"), core);
    QByteArray app(kXmlHeader);
    app += QStringLiteral("<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
                          "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
                          "<Application>ClassBoard</Application><Slides>%1</Slides></Properties>")
               .arg(n)
               .toUtf8();
    zip.addFile(QStringLiteral("docProps/app.xml"), app);

    // Presentation.
    QByteArray pres(kXmlHeader);
    pres += QStringLiteral("<p:presentation %1 saveSubsetFonts=\"1\">").arg(QString::fromLatin1(kNs)).toUtf8();
    pres += "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst><p:sldIdLst>";
    for (int i = 0; i < n; ++i)
        pres += QStringLiteral("<p:sldId id=\"%1\" r:id=\"rId%2\"/>").arg(256 + i).arg(i + 3).toUtf8();
    pres += QStringLiteral("</p:sldIdLst><p:sldSz cx=\"%1\" cy=\"%2\"/><p:notesSz cx=\"6858000\" cy=\"9144000\"/></p:presentation>")
                .arg(slideCx)
                .arg(slideCy)
                .toUtf8();
    zip.addFile(QStringLiteral("ppt/presentation.xml"), pres);

    QVector<QPair<QString, QString>> presRels = {
        {kRelBase + QStringLiteral("slideMaster"), QStringLiteral("slideMasters/slideMaster1.xml")},
        {kRelBase + QStringLiteral("theme"), QStringLiteral("theme/theme1.xml")}};
    for (int i = 1; i <= n; ++i)
        presRels.push_back({kRelBase + QStringLiteral("slide"), QStringLiteral("slides/slide%1.xml").arg(i)});
    zip.addFile(QStringLiteral("ppt/_rels/presentation.xml.rels"), rels(presRels));

    // Master, layout, theme.
    QByteArray master(kXmlHeader);
    master += QStringLiteral("<p:sldMaster %1><p:cSld><p:bg><p:bgRef idx=\"1001\"><a:schemeClr val=\"bg1\"/></p:bgRef></p:bg>"
                             "<p:spTree>%2</p:spTree></p:cSld>"
                             "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" accent1=\"accent1\" accent2=\"accent2\" "
                             "accent3=\"accent3\" accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" hlink=\"hlink\" folHlink=\"folHlink\"/>"
                             "<p:sldLayoutIdLst><p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/></p:sldLayoutIdLst>"
                             "<p:txStyles><p:titleStyle><a:lvl1pPr><a:defRPr sz=\"4400\"/></a:lvl1pPr></p:titleStyle>"
                             "<p:bodyStyle><a:lvl1pPr><a:defRPr sz=\"3200\"/></a:lvl1pPr></p:bodyStyle>"
                             "<p:otherStyle><a:lvl1pPr><a:defRPr sz=\"1800\"/></a:lvl1pPr></p:otherStyle></p:txStyles>"
                             "</p:sldMaster>")
                  .arg(QString::fromLatin1(kNs), QString::fromLatin1(kEmptyGroup))
                  .toUtf8();
    zip.addFile(QStringLiteral("ppt/slideMasters/slideMaster1.xml"), master);
    zip.addFile(QStringLiteral("ppt/slideMasters/_rels/slideMaster1.xml.rels"),
                rels({{kRelBase + QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout1.xml")},
                      {kRelBase + QStringLiteral("theme"), QStringLiteral("../theme/theme1.xml")}}));

    QByteArray layout(kXmlHeader);
    layout += QStringLiteral("<p:sldLayout %1 type=\"blank\" preserve=\"1\"><p:cSld name=\"Blank\"><p:spTree>%2</p:spTree></p:cSld>"
                             "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>")
                  .arg(QString::fromLatin1(kNs), QString::fromLatin1(kEmptyGroup))
                  .toUtf8();
    zip.addFile(QStringLiteral("ppt/slideLayouts/slideLayout1.xml"), layout);
    zip.addFile(QStringLiteral("ppt/slideLayouts/_rels/slideLayout1.xml.rels"),
                rels({{kRelBase + QStringLiteral("slideMaster"), QStringLiteral("../slideMasters/slideMaster1.xml")}}));
    zip.addFile(QStringLiteral("ppt/theme/theme1.xml"), themeXml());

    // Slides: one picture of the complete page each, fitted without cropping or stretching.
    for (int i = 1; i <= n; ++i) {
        const Slide& s = slides[i - 1];
        const QRect pic = pictureRect(slideEmu, s.pixelSize.isEmpty() ? first : s.pixelSize);
        const qint64 cx = pic.width();
        const qint64 cy = pic.height();
        const qint64 x = pic.x();
        const qint64 y = pic.y();
        const QString bg = s.background.isValid() ? s.background.name(QColor::HexRgb).mid(1).toUpper() : QStringLiteral("FFFFFF");
        QByteArray slide(kXmlHeader);
        slide += QStringLiteral("<p:sld %1><p:cSld><p:bg><p:bgPr><a:solidFill><a:srgbClr val=\"%6\"/></a:solidFill><a:effectLst/></p:bgPr></p:bg><p:spTree>%2"
                                "<p:pic><p:nvPicPr><p:cNvPr id=\"2\" name=\"Page %3\"/><p:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></p:cNvPicPr><p:nvPr/></p:nvPicPr>"
                                "<p:blipFill><a:blip r:embed=\"rId2\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
                                "<p:spPr><a:xfrm><a:off x=\"%7\" y=\"%8\"/><a:ext cx=\"%4\" cy=\"%5\"/></a:xfrm>"
                                "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>"
                                "</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>")
                     .arg(QString::fromLatin1(kNs), QString::fromLatin1(kEmptyGroup))
                     .arg(i)
                     .arg(cx)
                     .arg(cy)
                     .arg(bg)
                     .arg(x)
                     .arg(y)
                     .toUtf8();
        zip.addFile(QStringLiteral("ppt/slides/slide%1.xml").arg(i), slide);
        zip.addFile(QStringLiteral("ppt/slides/_rels/slide%1.xml.rels").arg(i),
                    rels({{kRelBase + QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout1.xml")},
                          {kRelBase + QStringLiteral("image"), QStringLiteral("../media/image%1.png").arg(i)}}));
        zip.addFile(QStringLiteral("ppt/media/image%1.png").arg(i), s.png, false);
    }
    return zip.finish();
}

QSize PptxExporter::slideSize(const QVector<Slide>& slides)
{
    const QSize first = !slides.isEmpty() && !slides.front().pixelSize.isEmpty() ? slides.front().pixelSize : QSize(16, 9);
    const qint64 cy = std::clamp<qint64>(qRound64(double(kSlideCx) * first.height() / first.width()), 914400, 51206400);
    return QSize(static_cast<int>(kSlideCx), static_cast<int>(cy));
}

QRect PptxExporter::pictureRect(const QSize& slide, const QSize& px)
{
    if (px.isEmpty())
        return QRect(QPoint(0, 0), slide);
    qint64 cx = slide.width();
    qint64 cy = qRound64(double(cx) * px.height() / px.width());
    if (cy > slide.height()) {
        cy = slide.height();
        cx = qRound64(double(cy) * px.width() / px.height());
    }
    return QRect(static_cast<int>((slide.width() - cx) / 2), static_cast<int>((slide.height() - cy) / 2), static_cast<int>(cx),
                 static_cast<int>(cy));
}

bool PptxExporter::exportPages(const DocumentSnapshot& snapshot, const QString& path, QString* error,
                               const ExportProgress& progress)
{
    if (snapshot.pages.empty()) {
        if (error)
            *error = tr("There are no pages to export.");
        return false;
    }
    QVector<Slide> slides;
    const int total = static_cast<int>(snapshot.pages.size());
    for (int i = 0; i < total; ++i) {
        const Page& page = *snapshot.pages[static_cast<size_t>(i)];
        const QImage image = ImageExporter::renderPage(page, snapshot.images, snapshot.coordinates, 2560);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        slides.push_back({bytes, image.size(), page.background().background});
        if (progress && !progress(i + 1, total + 1)) {
            if (error)
                *error = tr("Export cancelled.");
            return false;
        }
    }
    const QString title = snapshot.metadata.title.isEmpty() ? QStringLiteral("ClassBoard lesson") : snapshot.metadata.title;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(buildPackage(slides, title));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (progress)
        progress(total + 1, total + 1);
    return true;
}

// ========================================================================================== Image

QImage ImageExporter::renderPage(const Page& page, const ImageStore& images, const CoordinateSystem& coordinates,
                                 int pixelWidth)
{
    // The long side gets pixelWidth pixels so portrait pages are as sharp as landscape ones.
    const QRectF area = page.exportRect();
    QSize size;
    if (area.width() >= area.height())
        size = QSize(pixelWidth, std::max(1, qRound(pixelWidth * area.height() / area.width())));
    else
        size = QSize(std::max(1, qRound(pixelWidth * area.width() / area.height())), pixelWidth);
    return PageRenderer::renderToImage(page, area, size, images, coordinates, true);
}

bool ImageExporter::exportPage(const DocumentSnapshot& snapshot, int index, const QString& path, QString* error)
{
    if (index < 0 || index >= static_cast<int>(snapshot.pages.size())) {
        if (error)
            *error = tr("Invalid page.");
        return false;
    }
    const QImage image = renderPage(*snapshot.pages[static_cast<size_t>(index)], snapshot.images, snapshot.coordinates, 3840);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG")) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace cb
