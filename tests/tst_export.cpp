#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "export/Exporters.h"
#include "export/ZipWriter.h"
#include "app/ExportController.h"
#include "document/Commands.h"
#include "document/PageOperations.h"
#include "document/PageSize.h"
#include "storage/PageImport.h"
#include "storage/PdfImporter.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using namespace cb;

namespace {
quint32 le32(const QByteArray& b, int pos)
{
    return quint32(uchar(b[pos])) | (quint32(uchar(b[pos + 1])) << 8) | (quint32(uchar(b[pos + 2])) << 16)
        | (quint32(uchar(b[pos + 3])) << 24);
}
quint16 le16(const QByteArray& b, int pos) { return quint16(uchar(b[pos]) | (uchar(b[pos + 1]) << 8)); }

/// Reads the central directory of a ZIP archive and returns entry names.
QStringList zipEntries(const QByteArray& zip)
{
    QStringList names;
    const int eocd = zip.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0)
        return names;
    const int count = le16(zip, eocd + 10);
    int pos = static_cast<int>(le32(zip, eocd + 16));
    for (int i = 0; i < count; ++i) {
        if (le32(zip, pos) != 0x02014b50)
            return {};
        const int nameLen = le16(zip, pos + 28);
        const int extra = le16(zip, pos + 30);
        const int comment = le16(zip, pos + 32);
        names << QString::fromUtf8(zip.mid(pos + 46, nameLen));
        pos += 46 + nameLen + extra + comment;
    }
    return names;
}

/// Page sizes (points) of a PDF, read from its /MediaBox entries.
QVector<QSizeF> mediaBoxes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray bytes = f.readAll();
    // Binary PDF data contains NUL bytes: convert the whole buffer, not up to the first NUL.
    const QString text = QString::fromLatin1(bytes.constData(), bytes.size());
    QVector<QSizeF> out;
    QRegularExpression re(QStringLiteral("/MediaBox \\[\\s*([-0-9.]+)\\s+([-0-9.]+)\\s+([-0-9.]+)\\s+([-0-9.]+)\\s*\\]"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out.push_back(QSizeF(m.captured(3).toDouble() - m.captured(1).toDouble(), m.captured(4).toDouble() - m.captured(2).toDouble()));
    }
    return out;
}

template <typename Importer>
QVector<ImportedPage> runImport(Importer& importer, const QString& path, QString* error, int timeoutMs)
{
    QSignalSpy finished(&importer, &Importer::finished);
    importer.start(path, 60);
    if (!finished.wait(timeoutMs)) {
        if (error)
            *error = QStringLiteral("timeout");
        return {};
    }
    if (error)
        *error = finished.first().at(1).toString();
    return finished.first().at(0).template value<QVector<ImportedPage>>();
}

QVector<ImportedPage> importPdf(const QString& path, QString* error = nullptr)
{
    PdfImporter importer;
    return runImport(importer, path, error, 30000);
}

QVector<ImportedPage> importPresentation(const QString& path, QString* error = nullptr)
{
    PresentationImporter importer;
    return runImport(importer, path, error, 180000);
}

std::vector<ObjectPtr> oneObject(ObjectPtr o)
{
    std::vector<ObjectPtr> v;
    v.push_back(std::move(o));
    return v;
}

std::unique_ptr<DocumentSnapshot> sample()
{
    Document doc;
    Page* page = doc.currentPage();
    InkStyle ink;
    ink.width = 8;
    page->insertObject(0, StrokeObject::fromPagePoints({{QPointF(100, 100), 1}, {QPointF(800, 600), 1}}, ink));
    page->insertObject(1, TextObject::create(QStringLiteral("Photosynthesis"), QPointF(200, 200), TextFormat()));
    page->insertObject(2, ShapeObject::createBox(ShapeKind::Circle, QRectF(900, 300, 300, 300), ShapeStyle()));
    doc.insertPage(1, doc.createPage());
    return doc.snapshot();
}
} // namespace

class TestExport : public QObject
{
    Q_OBJECT
private slots:
    void zipWriterProducesValidArchive()
    {
        ZipWriter zip;
        zip.addFile(QStringLiteral("a.txt"), QByteArray(1000, 'a'));
        zip.addFile(QStringLiteral("dir/b.bin"), QByteArray("xyz"), false);
        const QByteArray data = zip.finish();
        QCOMPARE(zipEntries(data), (QStringList{QStringLiteral("a.txt"), QStringLiteral("dir/b.bin")}));
        // Deflated entry: method 8 and smaller than the input.
        QCOMPARE(le16(data, 8), quint16(8));
        QVERIFY(le32(data, 18) < 1000);
    }

    void pdfExport()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("lesson.pdf"));
        QString error;
        int lastProgress = 0;
        QVERIFY2(PdfExporter::exportPages(*sample(), path, &error, [&](int done, int) {
                     lastProgress = done;
                     return true;
                 }),
                 qPrintable(error));
        QCOMPARE(lastProgress, 2);
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bytes = f.readAll();
        QVERIFY(bytes.startsWith("%PDF"));
        QCOMPARE(bytes.count("/Type /Page\n") + bytes.count("/Type /Page\r") + bytes.count("/Type /Page "), 2);
    }

    void pptxExport()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("lesson.pptx"));
        QString error;
        QVERIFY2(PptxExporter::exportPages(*sample(), path, &error), qPrintable(error));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QStringList entries = zipEntries(f.readAll());
        for (const char* required : {"[Content_Types].xml", "_rels/.rels", "ppt/presentation.xml",
                                     "ppt/_rels/presentation.xml.rels", "ppt/slideMasters/slideMaster1.xml",
                                     "ppt/slideLayouts/slideLayout1.xml", "ppt/theme/theme1.xml", "ppt/slides/slide1.xml",
                                     "ppt/slides/slide2.xml", "ppt/media/image1.png", "ppt/media/image2.png"})
            QVERIFY2(entries.contains(QString::fromLatin1(required)), required);
    }

    void pngExport()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("page.png"));
        QString error;
        QVERIFY2(ImageExporter::exportPage(*sample(), 0, path, &error), qPrintable(error));
        const QImage img(path);
        QCOMPARE(img.width(), 3840);
        QCOMPARE(img.height(), 2160);
    }

    void writeSamplesForExternalValidation()
    {
        // CI / developers can open these in PowerPoint, LibreOffice or a PDF viewer.
        const QString dir = qEnvironmentVariable("CLASSBOARD_EXPORT_DIR");
        if (dir.isEmpty())
            QSKIP("Set CLASSBOARD_EXPORT_DIR to write sample exports");
        QString error;
        QVERIFY(PptxExporter::exportPages(*sample(), dir + QStringLiteral("/sample.pptx"), &error));
        QVERIFY(PdfExporter::exportPages(*sample(), dir + QStringLiteral("/sample.pdf"), &error));
    }

    void pdfPagesKeepTheirLogicalSize()
    {
        // Each PDF page has the page's own size: 16:9 board = 48 × 27 cm, A4 portrait = 21 × 29.7 cm.
        Document doc;
        pageops::newPage(doc, 0);
        pageops::setPageSize(doc, {1}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sizes.pdf"));
        QVERIFY(PdfExporter::exportPages(*doc.snapshot(), path, nullptr));
        const QVector<QSizeF> boxes = mediaBoxes(path);
        QCOMPARE(boxes.size(), 2);
        QVERIFY2(std::abs(boxes[0].width() - 480 / 25.4 * 72) < 1.5 && std::abs(boxes[0].height() - 270 / 25.4 * 72) < 1.5,
                 qPrintable(QStringLiteral("%1 x %2").arg(boxes[0].width()).arg(boxes[0].height())));
        QVERIFY2(std::abs(boxes[1].width() - 595.3) < 1.5 && std::abs(boxes[1].height() - 841.9) < 1.5,
                 qPrintable(QStringLiteral("%1 x %2").arg(boxes[1].width()).arg(boxes[1].height())));
    }

    void pptxSlidesFollowThePageAspect()
    {
        // First page A4 landscape: the slide gets that aspect; a 16:9 page is fitted whole.
        Document doc;
        pageops::setPageSize(doc, {0}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Landscape));
        pageops::newPage(doc, 0);
        pageops::setPageSize(doc, {1}, pagesize::sizeFor(pagesize::Preset::Board16x9, pagesize::Orientation::Landscape));
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("aspect.pptx"));
        QVERIFY(PptxExporter::exportPages(*doc.snapshot(), path, nullptr));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray zip = f.readAll();
        QVERIFY(zipEntries(zip).contains(QStringLiteral("ppt/slides/slide2.xml")));
        const QVector<PptxExporter::Slide> slides = {{QByteArray("png"), QSize(1188, 840), Qt::white},
                                                     {QByteArray("png"), QSize(1920, 1080), Qt::black}};
        const QSize slide = PptxExporter::slideSize(slides);
        QCOMPARE(slide.width(), 12192000);
        QVERIFY(std::abs(slide.width() / double(slide.height()) - 1188.0 / 840.0) < 1e-4);
        // The A4 page fills the slide exactly; the 16:9 page is fitted whole (full width, centred).
        QCOMPARE(PptxExporter::pictureRect(slide, QSize(1188, 840)), QRect(QPoint(0, 0), slide));
        const QRect wide = PptxExporter::pictureRect(slide, QSize(1920, 1080));
        QCOMPARE(wide.width(), slide.width());
        QVERIFY(std::abs(wide.width() / double(wide.height()) - 16.0 / 9.0) < 1e-4);
        QVERIFY(wide.top() > 0 && wide.bottom() < slide.height());
        // Default 16:9 lesson: standard 13.333 × 7.5 inch slide.
        QCOMPARE(PptxExporter::slideSize({{QByteArray(), QSize(1920, 1080), Qt::white}}), QSize(12192000, 6858000));
    }

    void exportRendersTheCompleteLogicalPage()
    {
        // The PNG covers the whole page (content at the corners included), independent of any view.
        Document doc;
        Page* page = doc.currentPage();
        InkStyle ink;
        ink.width = 20;
        ink.color = Qt::red;
        page->insertObject(0, StrokeObject::fromPagePoints({{QPointF(5, 5), 1}, {QPointF(40, 5), 1}}, ink));
        page->insertObject(1, StrokeObject::fromPagePoints({{QPointF(1880, 1075), 1}, {QPointF(1915, 1075), 1}}, ink));
        const QImage img = ImageExporter::renderPage(*page, doc.images(), doc.coordinates(), 1920);
        QCOMPARE(img.size(), QSize(1920, 1080));
        QCOMPARE(img.pixelColor(20, 5).red(), 255);
        QCOMPARE(img.pixelColor(1900, 1075).red(), 255);
        // A4 portrait page: portrait image with the page's aspect ratio.
        Document a4doc;
        pageops::setPageSize(a4doc, {0}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        page = a4doc.currentPage();
        page->insertObject(0, StrokeObject::fromPagePoints({{QPointF(10, 1180), 1}, {QPointF(60, 1180), 1}}, ink));
        const QImage a4 = ImageExporter::renderPage(*page, a4doc.images(), a4doc.coordinates(), 1188);
        QCOMPARE(a4.size(), QSize(840, 1188));
        QCOMPARE(a4.pixelColor(30, 1180).red(), 255);
        // The page background colour is exported behind the content.
        pageops::setBackgroundColor(a4doc, {0}, QColor(200, 220, 240));
        const QImage coloured = ImageExporter::renderPage(*a4doc.page(0), a4doc.images(), a4doc.coordinates(), 1188);
        QCOMPARE(coloured.pixelColor(400, 600), QColor(200, 220, 240));
        QCOMPARE(coloured.pixelColor(30, 1180).red(), 255); // content stays on top
        QTemporaryDir dir;
        QVERIFY(PdfExporter::exportPages(*a4doc.snapshot(), dir.filePath(QStringLiteral("bg.pdf")), nullptr));
        if (PdfImporter::isAvailable()) {
            // Rendered back from the PDF: A4 page, coloured background.
            const QVector<ImportedPage> pages = importPdf(dir.filePath(QStringLiteral("bg.pdf")));
            QCOMPARE(pages.size(), 1);
            const QImage rendered = QImage::fromData(pages.first().encoded);
            const QColor centre = rendered.pixelColor(rendered.width() / 2, rendered.height() / 2);
            QVERIFY2(std::abs(centre.red() - 200) < 4 && std::abs(centre.green() - 220) < 4 && std::abs(centre.blue() - 240) < 4,
                     qPrintable(centre.name()));
            QVERIFY(std::abs(pages.first().sizeMm.width() - 210) < 2 && std::abs(pages.first().sizeMm.height() - 297) < 2);
        }
    }

    void pdfImportRoundTrip()
    {
        if (!PdfImporter::isAvailable())
            QSKIP("No PDF backend (Qt PDF or Poppler's pdftoppm) on this machine");
        // Export a 16:9 page and an A4 page, import them again, edit, export again.
        Document source;
        pageops::newPage(source, 0);
        pageops::setPageSize(source, {1}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        source.page(1)->insertObject(0, TextObject::create(QStringLiteral("Worksheet"), QPointF(420, 200), TextFormat()));
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("roundtrip.pdf"));
        QVERIFY(PdfExporter::exportPages(*source.snapshot(), path, nullptr));

        const QVector<ImportedPage> pages = importPdf(path);
        QCOMPARE(pages.size(), 2);
        QVERIFY(std::abs(pages[0].sizeMm.width() - 480) < 2 && std::abs(pages[0].sizeMm.height() - 270) < 2);
        QVERIFY(std::abs(pages[1].sizeMm.width() - 210) < 2 && std::abs(pages[1].sizeMm.height() - 297) < 2);

        Document doc;
        QCOMPARE(insertImportedPages(doc, 0, pages, QStringLiteral("roundtrip"), ImportSizing::Physical, QStringLiteral("Import PDF")), 2);
        QCOMPARE(doc.pageCount(), 3);
        // Dimensions and aspect ratio preserved, the whole source page shown, not stretched.
        const Page* a4 = doc.page(2);
        QVERIFY(std::abs(a4->size().width() - 840) < 4 && std::abs(a4->size().height() - 1188) < 6);
        QCOMPARE(a4->objectCount(), 1);
        const QRectF imageBounds = a4->objectAt(0)->sceneBounds();
        QVERIFY(std::abs(imageBounds.width() - a4->size().width()) < 0.5);
        QVERIFY(std::abs(imageBounds.height() - a4->size().height()) < 0.5);
        const double imageAspect = pages[1].pixelSize.width() / double(pages[1].pixelSize.height());
        QVERIFY(std::abs(a4->size().width() / a4->size().height() - imageAspect) < 1e-6);
        QCOMPARE(a4->background().background, QColor(Qt::white));
        // One undo step removes the import.
        doc.commands().undo();
        QCOMPARE(doc.pageCount(), 1);
        doc.commands().redo();

        // Edit (write on the imported page) and export again: same page sizes, annotations included.
        InkStyle ink;
        ink.color = Qt::blue;
        ink.width = 12;
        doc.commands().push(std::make_unique<AddObjectsCommand>(
            doc.page(2)->id(), oneObject(StrokeObject::fromPagePoints({{QPointF(100, 100), 1}, {QPointF(700, 100), 1}}, ink))));
        const QString again = dir.filePath(QStringLiteral("again.pdf"));
        QVERIFY(PdfExporter::exportPages(*doc.snapshot({1, 2}), again, nullptr));
        const QVector<QSizeF> boxes = mediaBoxes(again);
        QCOMPARE(boxes.size(), 2);
        QVERIFY(std::abs(boxes[1].width() - 595.3) < 3 && std::abs(boxes[1].height() - 841.9) < 4);
        const QImage png = ImageExporter::renderPage(*doc.page(2), doc.images(), doc.coordinates(), 1188);
        QCOMPARE(png.pixelColor(400, 100).blue(), 255);
    }

    void invalidPdfReportsAnError()
    {
        if (!PdfImporter::isAvailable())
            QSKIP("No PDF backend on this machine");
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("broken.pdf"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("This is not a PDF file at all.");
        f.close();
        QString error;
        const QVector<ImportedPage> pages = importPdf(path, &error);
        QVERIFY(pages.isEmpty());
        QVERIFY2(error.contains(QStringLiteral("could not be opened")), qPrintable(error));
        // Missing file.
        importPdf(dir.filePath(QStringLiteral("missing.pdf")), &error);
        QVERIFY(error.contains(QStringLiteral("does not exist")));
    }

    void pptxImport()
    {
        if (!PresentationImporter::isAvailable())
            QSKIP("No PowerPoint or LibreOffice converter on this machine");
        // A real 3-slide 4:3 presentation written by ClassBoard's own exporter.
        Document source;
        pageops::setPageSize(source, {0}, pagesize::sizeFor(pagesize::Preset::Board4x3, pagesize::Orientation::Landscape));
        pageops::newPage(source, 0);
        pageops::newPage(source, 1);
        pageops::setPageSize(source, {}, pagesize::sizeFor(pagesize::Preset::Board4x3, pagesize::Orientation::Landscape));
        QTemporaryDir dir;
        const QString pptx = dir.filePath(QStringLiteral("slides.pptx"));
        QVERIFY(PptxExporter::exportPages(*source.snapshot(), pptx, nullptr));
        QString error;
        const QVector<ImportedPage> pages = importPresentation(pptx, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(pages.size(), 3);
        for (const ImportedPage& p : pages)
            QVERIFY2(std::abs(p.pixelSize.width() / double(p.pixelSize.height()) - 4.0 / 3.0) < 0.01,
                     qPrintable(QStringLiteral("%1x%2").arg(p.pixelSize.width()).arg(p.pixelSize.height())));
        Document doc;
        QCOMPARE(insertImportedPages(doc, 0, pages, QStringLiteral("slides"), ImportSizing::BoardSized, QStringLiteral("Import")), 3);
        QCOMPARE(doc.page(1)->size().width(), 1920.0);
        QVERIFY(std::abs(doc.page(1)->size().height() - 1440) < 1440 * 0.005); // 4:3 within pixel rounding
        // Round trip: export the imported slides again as a presentation.
        QVERIFY(PptxExporter::exportPages(*doc.snapshot({1, 2, 3}), dir.filePath(QStringLiteral("back.pptx")), nullptr));
    }

    void pptImport()
    {
        if (!PresentationImporter::isAvailable() || PresentationImporter::availableConverter() != PresentationImporter::Converter::LibreOffice)
            QSKIP("Needs LibreOffice to create the legacy .ppt test file");
        QTemporaryDir dir;
        const QString pptx = dir.filePath(QStringLiteral("legacy.pptx"));
        QVERIFY(PptxExporter::exportPages(*sample(), pptx, nullptr));
        // Make a binary PowerPoint 97-2003 file from it.
        QProcess convert;
        convert.start(PresentationImporter::libreOfficePath(),
                      {QStringLiteral("-env:UserInstallation=") + QUrl::fromLocalFile(dir.filePath(QStringLiteral("p"))).toString(),
                       QStringLiteral("--headless"), QStringLiteral("--convert-to"), QStringLiteral("ppt"), QStringLiteral("--outdir"),
                       dir.path(), pptx});
        QVERIFY(convert.waitForFinished(120000));
        const QString ppt = dir.filePath(QStringLiteral("legacy.ppt"));
        QVERIFY(QFileInfo::exists(ppt));
        QString error;
        const QVector<ImportedPage> pages = importPresentation(ppt, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(pages.size(), 2);
        QVERIFY(std::abs(pages[0].pixelSize.width() / double(pages[0].pixelSize.height()) - 16.0 / 9.0) < 0.01);
    }

    void presentationImportRejectsOtherFiles()
    {
        QTemporaryDir dir;
        QString error;
        const QString txt = dir.filePath(QStringLiteral("notes.txt"));
        QFile f(txt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
        f.close();
        importPresentation(txt, &error);
        QVERIFY(!error.isEmpty());
        // Files with a presentation extension but other content are rejected before conversion
        // (office suites would otherwise turn e.g. plain text into a page).
        for (const char* name : {"fake.pptx", "fake.ppt"}) {
            const QString fake = dir.filePath(QString::fromLatin1(name));
            QFile g(fake);
            QVERIFY(g.open(QIODevice::WriteOnly));
            g.write("not a presentation");
            g.close();
            const QVector<ImportedPage> pages = importPresentation(fake, &error);
            QVERIFY(pages.isEmpty());
            QVERIFY2(error.contains(QStringLiteral("not a real PowerPoint file")), qPrintable(error));
        }
        if (PresentationImporter::isAvailable()) {
            const QString fake = dir.filePath(QStringLiteral("fake.pptx"));
            QFile g(fake);
            QVERIFY(g.open(QIODevice::WriteOnly));
            g.write("not a zip");
            g.close();
            const QVector<ImportedPage> pages = importPresentation(fake, &error);
            QVERIFY(pages.isEmpty());
            QVERIFY(!error.isEmpty());
        }
    }

    void pageRanges()
    {
        QVector<int> pages;
        QVERIFY(ExportController::parseRange(QStringLiteral("1-3, 5"), 6, &pages));
        QCOMPARE(pages, (QVector<int>{0, 1, 2, 4}));
        QVERIFY(ExportController::parseRange(QStringLiteral("4-"), 6, &pages));
        QCOMPARE(pages, (QVector<int>{3, 4, 5}));
        QVERIFY(ExportController::parseRange(QStringLiteral("2 2 1"), 3, &pages));
        QCOMPARE(pages, (QVector<int>{0, 1}));
        QVERIFY(!ExportController::parseRange(QStringLiteral("0"), 3, &pages));
        QVERIFY(!ExportController::parseRange(QStringLiteral("2-9"), 3, &pages));
        QVERIFY(!ExportController::parseRange(QStringLiteral("abc"), 3, &pages));
    }
};

QTEST_MAIN(TestExport)
#include "tst_export.moc"
