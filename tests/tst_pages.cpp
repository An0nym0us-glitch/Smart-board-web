// Page management: clear / delete / duplicate / new, page sizes, background colours and their
// persistence in the .classboard format.
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ImageObject.h"
#include "document/PageOperations.h"
#include "document/PageSize.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "document/ObjectFactory.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "graph/GraphObject.h"
#include "storage/ProjectSerializer.h"

#include <QImage>
#include <QtTest>

using namespace cb;

namespace {
/// Fills a page with one object of every kind the teacher can place (ink, shape, image, graph,
/// text, vector, measurement and an image as it is created by imports).
void fillPage(Document& doc, Page& page)
{
    std::vector<ObjectPtr> objs;
    objs.push_back(StrokeObject::fromPagePoints({{QPointF(10, 10), 1.0f}, {QPointF(200, 60), 1.0f}}, InkStyle()));
    objs.push_back(ShapeObject::createBox(ShapeKind::Rectangle, QRectF(300, 300, 200, 100), ShapeStyle()));
    QImage img(64, 48, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    const QString key = doc.images().addImage(img);
    objs.push_back(ImageObject::create(key, QSizeF(128, 96), QPointF(700, 500)));
    objs.push_back(GraphObject::create(QPointF(1200, 500), QSizeF(500, 400)));
    objs.push_back(ObjectFactory::createText(QStringLiteral("Hello"), QPointF(900, 200)));
    objs.push_back(GeometryObject::create(ConstructKind::Vector, {QPointF(100, 800), QPointF(300, 700)}, Qt::cyan, true));
    objs.push_back(MeasurementObject::create(MeasureKind::Distance, {QPointF(100, 900), QPointF(500, 900)}, Qt::yellow));
    doc.commands().push(std::make_unique<AddObjectsCommand>(page.id(), std::move(objs), QStringLiteral("fill")));
}
} // namespace

class TestPages : public QObject
{
    Q_OBJECT
private slots:
    void clearPageRemovesEveryKindOfContentButKeepsThePage()
    {
        Document doc;
        pageops::newPage(doc, 0);
        QCOMPARE(doc.pageCount(), 2);
        Page* first = doc.page(0);
        Page* second = doc.page(1);
        fillPage(doc, *first);
        fillPage(doc, *second);
        QCOMPARE(first->objectCount(), 7);
        const TemplateSpec background = pageops::withBackgroundColor(first->background(), QColor(Qt::white));
        doc.commands().push(std::make_unique<ModifyPageCommand>(first->id(), QStringLiteral("Named"), background, QString()));
        pageops::setPageSize(doc, {0}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        const PageId id = first->id();
        const QSizeF size = first->size();

        QVERIFY(pageops::clearPage(doc, 0));
        QCOMPARE(doc.pageCount(), 2);           // the page is not deleted
        QCOMPARE(doc.page(0)->id(), id);        // it is the same page
        QCOMPARE(doc.page(0)->objectCount(), 0); // but blank
        QCOMPARE(doc.page(0)->size(), size);    // size, background and name are kept
        QCOMPARE(doc.page(0)->background(), background);
        QCOMPARE(doc.page(0)->name(), QStringLiteral("Named"));
        QCOMPARE(doc.page(1)->objectCount(), 7); // other pages are untouched
        QVERIFY(!pageops::clearPage(doc, 0));    // nothing left to clear

        QCOMPARE(doc.commands().undoText(), QStringLiteral("Clear page"));
        doc.commands().undo();
        QCOMPARE(doc.page(0)->objectCount(), 7);
        doc.commands().redo();
        QCOMPARE(doc.page(0)->objectCount(), 0);
    }

    void deletePageUsesTheExistingCommandAndKeepsOnePage()
    {
        Document doc;
        pageops::newPage(doc, 0);
        pageops::newPage(doc, 1);
        QCOMPARE(doc.pageCount(), 3);
        const PageId middle = doc.page(1)->id();
        fillPage(doc, *doc.page(1));
        QVERIFY(pageops::deletePage(doc, 1));
        QCOMPARE(doc.pageCount(), 2);
        QCOMPARE(doc.indexOfPage(middle), -1);
        doc.commands().undo();
        QCOMPARE(doc.pageCount(), 3);
        QCOMPARE(doc.page(1)->id(), middle);
        QCOMPARE(doc.page(1)->objectCount(), 7);
        QVERIFY(pageops::deletePage(doc, 0));
        QVERIFY(pageops::deletePage(doc, 0));
        QCOMPARE(doc.pageCount(), 1);
        QVERIFY(!pageops::deletePage(doc, 0)); // a lesson keeps at least one page
        QCOMPARE(doc.pageCount(), 1);
    }

    void duplicateAndNewPage()
    {
        Document doc;
        fillPage(doc, *doc.page(0));
        QVERIFY(pageops::duplicatePage(doc, 0));
        QCOMPARE(doc.pageCount(), 2);
        QCOMPARE(doc.currentPageIndex(), 1);
        QCOMPARE(doc.page(1)->objectCount(), 7);
        QVERIFY(doc.page(1)->id() != doc.page(0)->id());
        doc.setDefaultPageSize(pagesize::sizeFor(pagesize::Preset::Board4x3, pagesize::Orientation::Landscape));
        pageops::newPage(doc, 1);
        QCOMPARE(doc.page(2)->objectCount(), 0);
        QCOMPARE(doc.page(2)->size(), QSizeF(1440, 1080));
    }

    void pageSizePresets()
    {
        using namespace pagesize;
        QCOMPARE(sizeFor(Preset::Board16x9, Orientation::Landscape), QSizeF(1920, 1080));
        QCOMPARE(sizeFor(Preset::Board4x3, Orientation::Landscape), QSizeF(1440, 1080));
        QCOMPARE(sizeFor(Preset::A4, Orientation::Portrait), QSizeF(840, 1188));
        QCOMPARE(sizeFor(Preset::A4, Orientation::Landscape), QSizeF(1188, 840));
        QCOMPARE(sizeFor(Preset::A3, Orientation::Portrait), QSizeF(1188, 1680));
        QCOMPARE(presetOf(QSizeF(840, 1188)), Preset::A4);
        QCOMPARE(presetOf(QSizeF(1188, 840)), Preset::A4);
        QCOMPARE(presetOf(QSizeF(1920, 1080)), Preset::Board16x9);
        QCOMPARE(presetOf(QSizeF(1000, 1000)), Preset::Custom);
        QCOMPARE(fromCentimetres(QSizeF(30, 20)), QSizeF(1200, 800));
        QCOMPARE(toCentimetres(QSizeF(840, 1188)), QSizeF(21, 29.7));
        // Page size is logical: 1 cm on the page is 1 cm of the measurement system.
        QCOMPARE(kUnitsPerCm, Document().coordinates().pxPerUnit());
        // Imported content keeps its aspect ratio.
        const QSizeF slide = forAspect(4, 3);
        QCOMPARE(slide.width() / slide.height(), 4.0 / 3.0);
        const QSizeF portrait = forAspect(210, 297);
        QVERIFY(std::abs(portrait.width() / portrait.height() - 210.0 / 297.0) < 1e-9);
    }

    void pageSizeChangeIsUndoableAndKeepsContent()
    {
        Document doc;
        fillPage(doc, *doc.page(0));
        const QRectF before = doc.page(0)->objectAt(1)->sceneBounds();
        const QSizeF a4 = pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait);
        QVERIFY(pageops::setPageSize(doc, {0}, a4));
        QCOMPARE(doc.page(0)->size(), a4);
        QCOMPARE(doc.page(0)->frameRect(), QRectF(0, 0, 840, 1188));
        QCOMPARE(doc.page(0)->objectAt(1)->sceneBounds(), before); // content keeps document coordinates
        doc.commands().undo();
        QCOMPARE(doc.page(0)->size(), QSizeF(1920, 1080));
        // All pages at once.
        pageops::newPage(doc, 0);
        QVERIFY(pageops::setPageSize(doc, {}, a4));
        QCOMPARE(doc.page(0)->size(), a4);
        QCOMPARE(doc.page(1)->size(), a4);
        QVERIFY(!pageops::setPageSize(doc, {}, a4)); // unchanged -> no command
    }

    void backgroundColours()
    {
        Document doc;
        pageops::newPage(doc, 0);
        const TemplateSpec original = doc.page(1)->background();
        QVERIFY(pageops::setBackgroundColor(doc, {0}, QColor(Qt::white)));
        QCOMPARE(doc.page(0)->background().background, QColor(Qt::white));
        QCOMPARE(doc.page(1)->background(), original); // per page
        QVERIFY(!doc.page(0)->background().isDark());
        QVERIFY(doc.page(0)->background().lineColor.lightness() < 128); // lines stay visible on white
        QVERIFY(pageops::setBackgroundColor(doc, {1}, QColor(Qt::black)));
        QCOMPARE(doc.page(1)->background().background, QColor(Qt::black));
        QVERIFY(pageops::setBackgroundColor(doc, {0}, QColor(0xd9, 0xd9, 0xd9)));
        QCOMPARE(doc.page(0)->background().background, QColor(0xd9, 0xd9, 0xd9));
        QVERIFY(pageops::setBackgroundColor(doc, {0}, QColor(12, 60, 140))); // custom colour
        QCOMPARE(doc.page(0)->background().background, QColor(12, 60, 140));
        // A pattern is preserved when only the colour changes.
        TemplateSpec grid = doc.page(0)->background();
        grid.kind = TemplateKind::Grid;
        doc.commands().push(std::make_unique<ModifyPageCommand>(doc.page(0)->id(), QString(), grid, QString()));
        QVERIFY(pageops::setBackgroundColor(doc, {0}, QColor(Qt::white)));
        QCOMPARE(doc.page(0)->background().kind, TemplateKind::Grid);
        // Undo restores the previous background.
        doc.commands().undo();
        QCOMPARE(doc.page(0)->background().background, QColor(12, 60, 140));
        // The background is a page property, not an object: nothing is selectable or cleared.
        QCOMPARE(doc.page(0)->objectCount(), 0);
        QVERIFY(!doc.page(0)->topmostAt(QPointF(500, 500), 10));
    }

    void sizesBackgroundsAndScaleArePersisted()
    {
        Document doc;
        pageops::newPage(doc, 0);
        pageops::setPageSize(doc, {1}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        pageops::setBackgroundColor(doc, {1}, QColor(200, 200, 200));
        doc.setDefaultPageSize(pagesize::sizeFor(pagesize::Preset::Board4x3, pagesize::Orientation::Landscape));
        MeasureScale scale;
        scale.boardValue = 10;
        scale.boardUnit = QStringLiteral("cm");
        scale.realValue = 1;
        scale.realUnit = QStringLiteral("km");
        doc.commands().push(std::make_unique<SetScaleCommand>(scale, QStringLiteral("Scale")));

        const QByteArray bytes = ProjectSerializer::serialize(doc.copyContents());
        DocumentContents loaded;
        QString error;
        QVERIFY2(ProjectSerializer::deserialize(bytes, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.pages.size(), size_t(2));
        QCOMPARE(loaded.pages[1]->size(), QSizeF(840, 1188));
        QCOMPARE(loaded.pages[1]->background().background, QColor(200, 200, 200));
        QCOMPARE(loaded.pages[0]->size(), QSizeF(1920, 1080));
        QCOMPARE(loaded.defaultPageSize, QSizeF(1440, 1080));
        QCOMPARE(loaded.coordinates.scale(), scale);
        QVERIFY(loaded.coordinates.usesScale());
    }

    void coordinateOriginFollowsThePageCentre()
    {
        Document doc;
        Page* page = doc.page(0);
        // Default board: the stored system unchanged, origin in the centre.
        QCOMPARE(doc.coordinatesFor(page).toMath(QPointF(960, 540)), QPointF(0, 0));
        pageops::setPageSize(doc, {0}, pagesize::sizeFor(pagesize::Preset::A4, pagesize::Orientation::Portrait));
        const CoordinateSystem cs = doc.coordinatesFor(page);
        QCOMPARE(cs.toMath(QPointF(420, 594)), QPointF(0, 0));
        // Units are unchanged: 40 units = 1 cm.
        QCOMPARE(cs.mathDistance(QPointF(0, 0), QPointF(40, 0)), 1.0);
    }
};

QTEST_MAIN(TestPages)
#include "tst_pages.moc"
