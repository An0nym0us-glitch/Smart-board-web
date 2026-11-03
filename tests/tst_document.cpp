#include "document/Commands.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"

#include <QSignalSpy>
#include <QtTest>

using namespace cb;

namespace {
std::unique_ptr<StrokeObject> line(qreal y)
{
    return StrokeObject::fromPagePoints({{QPointF(0, y), 1.0f}, {QPointF(100, y), 1.0f}}, InkStyle());
}

std::vector<ObjectPtr> one(ObjectPtr o)
{
    std::vector<ObjectPtr> v;
    v.push_back(std::move(o));
    return v;
}
} // namespace

class TestDocument : public QObject
{
    Q_OBJECT
private slots:
    void newDocumentHasOnePage()
    {
        Document doc;
        QCOMPARE(doc.pageCount(), 1);
        QVERIFY(!doc.isModified());
        QVERIFY(!doc.commands().canUndo());
    }

    void addUndoRedo()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        QSignalSpy added(&doc, &Document::objectAdded);
        doc.commands().push(std::make_unique<AddObjectsCommand>(pid, one(line(10))));
        QCOMPARE(doc.currentPage()->objectCount(), 1);
        QCOMPARE(added.count(), 1);
        QVERIFY(doc.isModified());
        doc.commands().undo();
        QCOMPARE(doc.currentPage()->objectCount(), 0);
        QVERIFY(!doc.isModified());
        doc.commands().redo();
        QCOMPARE(doc.currentPage()->objectCount(), 1);
    }

    void removeRestoresOrder()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        std::vector<ObjectPtr> objs;
        for (int i = 0; i < 5; ++i)
            objs.push_back(line(i * 10));
        std::vector<ObjectId> ids;
        for (auto& o : objs)
            ids.push_back(o->id());
        doc.commands().push(std::make_unique<AddObjectsCommand>(pid, std::move(objs)));
        doc.commands().push(std::make_unique<RemoveObjectsCommand>(pid, std::vector<ObjectId>{ids[3], ids[1]}));
        QCOMPARE(doc.currentPage()->objectCount(), 3);
        doc.commands().undo();
        for (int i = 0; i < 5; ++i)
            QCOMPARE(doc.currentPage()->objectAt(i)->id(), ids[static_cast<size_t>(i)]);
    }

    void modifyCommand()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        auto s = line(0);
        const ObjectId id = s->id();
        doc.commands().push(std::make_unique<AddObjectsCommand>(pid, one(std::move(s))));
        DocumentObject* o = doc.currentPage()->object(id);
        auto after = o->clone();
        after->setPosition(o->position() + QPointF(50, 0));
        auto cmd = std::make_unique<ModifyObjectsCommand>(pid);
        cmd->add(o->clone(), std::move(after));
        doc.commands().push(std::move(cmd));
        QCOMPARE(doc.currentPage()->object(id)->position(), QPointF(100, 0));
        doc.commands().undo();
        QCOMPARE(doc.currentPage()->object(id)->position(), QPointF(50, 0));
    }

    void replaceCommandSplitsAndRestores()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        std::vector<ObjectPtr> objs;
        objs.push_back(line(0));
        objs.push_back(line(50));
        objs.push_back(line(100));
        const ObjectId middle = objs[1]->id();
        const ObjectId first = objs[0]->id();
        const ObjectId last = objs[2]->id();
        doc.commands().push(std::make_unique<AddObjectsCommand>(pid, std::move(objs)));

        // Erase the middle stroke's centre: it becomes two fragments at index 1 and 2.
        Page* page = doc.currentPage();
        auto* stroke = static_cast<StrokeObject*>(page->object(middle));
        bool touched = false;
        auto fragments = stroke->eraseCircle(QPointF(50, 50), 10, &touched);
        QVERIFY(touched);
        QCOMPARE(static_cast<int>(fragments.size()), 2);
        std::vector<ReplaceObjectsCommand::Removed> removed;
        ReplaceObjectsCommand::Removed r;
        r.index = 1;
        r.object = doc.takeObject(pid, middle);
        removed.push_back(std::move(r));
        std::vector<ReplaceObjectsCommand::Added> added;
        int index = 1;
        for (auto& f : fragments) {
            added.push_back({f->id(), index});
            doc.insertObject(pid, index++, std::move(f));
        }
        doc.commands().pushApplied(std::make_unique<ReplaceObjectsCommand>(pid, std::move(removed), std::move(added), QStringLiteral("Erase")));
        QCOMPARE(page->objectCount(), 4);
        doc.commands().undo();
        QCOMPARE(page->objectCount(), 3);
        QCOMPARE(page->objectAt(0)->id(), first);
        QCOMPARE(page->objectAt(1)->id(), middle);
        QCOMPARE(page->objectAt(2)->id(), last);
        doc.commands().redo();
        QCOMPARE(page->objectCount(), 4);
        QCOMPARE(page->objectAt(3)->id(), last);
    }

    void pages()
    {
        Document doc;
        doc.commands().push(std::make_unique<InsertPageCommand>(1, doc.createPage()));
        doc.commands().push(std::make_unique<InsertPageCommand>(2, doc.createPage()));
        QCOMPARE(doc.pageCount(), 3);
        QCOMPARE(doc.currentPageIndex(), 2);
        const PageId third = doc.page(2)->id();
        doc.commands().push(std::make_unique<MovePageCommand>(2, 0));
        QCOMPARE(doc.page(0)->id(), third);
        QCOMPARE(doc.currentPageIndex(), 0);
        doc.commands().push(std::make_unique<RemovePageCommand>(0));
        QCOMPARE(doc.pageCount(), 2);
        doc.commands().undo();
        QCOMPARE(doc.pageCount(), 3);
        QCOMPARE(doc.page(0)->id(), third);
        doc.commands().undo();
        QCOMPARE(doc.page(2)->id(), third);
    }

    void lastPageCannotBeRemoved()
    {
        Document doc;
        QVERIFY(!doc.takePage(0));
        QCOMPARE(doc.pageCount(), 1);
    }

    void modifyPageBackground()
    {
        Document doc;
        Page* page = doc.currentPage();
        TemplateSpec grid;
        grid.id = QStringLiteral("grid");
        grid.kind = TemplateKind::Grid;
        doc.commands().push(std::make_unique<ModifyPageCommand>(page->id(), QStringLiteral("Intro"), grid, QStringLiteral("t")));
        QCOMPARE(page->background().kind, TemplateKind::Grid);
        QCOMPARE(page->name(), QStringLiteral("Intro"));
        doc.commands().undo();
        QCOMPARE(page->background().kind, TemplateKind::Blank);
        QVERIFY(page->name().isEmpty());
    }

    void cleanStateAndLimit()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        doc.commands().setLimit(10);
        for (int i = 0; i < 25; ++i)
            doc.commands().push(std::make_unique<AddObjectsCommand>(pid, one(line(i))));
        QCOMPARE(doc.commands().count(), 10);
        doc.markSaved();
        QVERIFY(!doc.isModified());
        doc.commands().undo();
        QVERIFY(doc.isModified());
        doc.commands().redo();
        QVERIFY(!doc.isModified());
        doc.commands().setCleanIndex(-1);
        QVERIFY(doc.isModified());
    }

    void compositeUndoesInReverse()
    {
        Document doc;
        auto macro = std::make_unique<CompositeCommand>(QStringLiteral("two pages"));
        auto a = std::make_unique<InsertPageCommand>(1, doc.createPage());
        a->redo(doc);
        macro->add(std::move(a));
        auto b = std::make_unique<InsertPageCommand>(2, doc.createPage());
        b->redo(doc);
        macro->add(std::move(b));
        doc.commands().pushApplied(std::move(macro));
        QCOMPARE(doc.pageCount(), 3);
        doc.commands().undo();
        QCOMPARE(doc.pageCount(), 1);
    }

    void snapshotIsIndependent()
    {
        Document doc;
        const PageId pid = doc.currentPage()->id();
        doc.commands().push(std::make_unique<AddObjectsCommand>(pid, one(line(0))));
        auto snap = doc.snapshot();
        doc.commands().undo();
        QCOMPARE(snap->pages.front()->objectCount(), 1);
        QCOMPARE(doc.currentPage()->objectCount(), 0);
    }

    void exportRectKeepsAspect()
    {
        Page page;
        QCOMPARE(page.exportRect(), page.frameRect());
        page.insertObject(0, ShapeObject::createBox(ShapeKind::Rectangle, QRectF(1800, 900, 600, 600), ShapeStyle()));
        const QRectF r = page.exportRect();
        QVERIFY(r.contains(page.contentBounds()));
        QVERIFY(qAbs(r.width() / r.height() - 16.0 / 9.0) < 1e-6);
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "tst_document.moc"
