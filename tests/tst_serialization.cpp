#include "document/Document.h"
#include "document/ImageObject.h"
#include "document/ObjectFactory.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "document/TextObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "graph/GraphObject.h"
#include "graph/TableObject.h"
#include "math/equation/EquationObject.h"
#include "storage/AutosaveManager.h"
#include "storage/Container.h"
#include "storage/ProjectSerializer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace cb;

class TestSerialization : public QObject
{
    Q_OBJECT
private:
    static DocumentContents sampleContents()
    {
        DocumentContents c;
        auto page = std::make_unique<Page>();
        page->setName(QStringLiteral("Fractions"));
        InkStyle ink;
        ink.color = QColor(255, 213, 79);
        ink.width = 6;
        ink.pressure = true;
        page->insertObject(0, StrokeObject::fromPagePoints({{QPointF(10, 10), 0.3f}, {QPointF(50, 60), 0.8f}, {QPointF(90, 20), 1.0f}}, ink));
        ShapeStyle style;
        style.fill = QColor(100, 200, 255, 60);
        page->insertObject(1, ShapeObject::createBox(ShapeKind::Hexagon, QRectF(200, 200, 120, 100), style));
        page->insertObject(2, ShapeObject::createLine(ShapeKind::Arrow, QPointF(0, 0), QPointF(100, 50), style));
        page->insertObject(3, ShapeObject::createPolygon({QPointF(0, 0), QPointF(50, 0), QPointF(20, 40)}, style));
        TextFormat tf;
        tf.bold = true;
        tf.pixelSize = 36;
        page->insertObject(4, TextObject::create(QStringLiteral("Hello class\nline two"), QPointF(300, 400), tf));
        page->insertObject(5, EquationObject::create(QStringLiteral("\\frac{a}{b} + \\sqrt{x^2}"), QPointF(600, 300), 48, Qt::white));
        auto graph = GraphObject::create(QPointF(900, 500), QSizeF(600, 400));
        graph->addFunction(QStringLiteral("a*x^2"), QColor());
        graph->setParameterValue(QStringLiteral("a"), 2.5);
        graph->addPoint(QPointF(1, 2.5), QStringLiteral("P"));
        graph->addVector(QPointF(0, 0), QPointF(3, 2));
        page->insertObject(6, std::move(graph));
        auto table = TableObject::create(3, 2, QPointF(100, 800));
        table->setCell(0, 0, QStringLiteral("x"));
        table->setCell(2, 1, QStringLiteral("42"));
        page->insertObject(7, std::move(table));
        page->insertObject(8, GeometryObject::create(ConstructKind::Vector, {QPointF(0, 0), QPointF(80, -40)}, Qt::cyan, true));
        page->insertObject(9, MeasurementObject::create(MeasureKind::Angle, {QPointF(100, 0), QPointF(0, 0), QPointF(0, 100)}, Qt::yellow));
        QImage img(64, 48, QImage::Format_ARGB32);
        img.fill(Qt::red);
        const QString key = c.images.addImage(img);
        page->insertObject(10, ImageObject::create(key, QSizeF(128, 96), QPointF(500, 700)));
        TemplateSpec spec;
        spec.id = QStringLiteral("grid");
        spec.kind = TemplateKind::Grid;
        page->setBackground(spec);
        c.pages.push_back(std::move(page));
        c.pages.push_back(std::make_unique<Page>());
        c.metadata.title = QStringLiteral("Test lesson");
        c.currentPage = 1;
        return c;
    }

private slots:
    void roundTripAllObjects()
    {
        const DocumentContents original = sampleContents();
        const QByteArray bytes = ProjectSerializer::serialize(original);
        DocumentContents loaded;
        QString error;
        int skipped = -1;
        QVERIFY2(ProjectSerializer::deserialize(bytes, &loaded, &error, &skipped), qPrintable(error));
        QCOMPARE(skipped, 0);
        QCOMPARE(loaded.pages.size(), original.pages.size());
        QCOMPARE(loaded.metadata.title, QStringLiteral("Test lesson"));
        QCOMPARE(loaded.currentPage, 1);
        const Page& a = *original.pages[0];
        const Page& b = *loaded.pages[0];
        QCOMPARE(b.name(), QStringLiteral("Fractions"));
        QCOMPARE(b.background().kind, TemplateKind::Grid);
        QCOMPARE(b.objectCount(), a.objectCount());
        for (int i = 0; i < a.objectCount(); ++i) {
            QCOMPARE(b.objectAt(i)->type(), a.objectAt(i)->type());
            QCOMPARE(b.objectAt(i)->id(), a.objectAt(i)->id());
            // Serialising the loaded object must give identical JSON.
            QCOMPARE(b.objectAt(i)->toJson(), a.objectAt(i)->toJson());
        }
        QCOMPARE(loaded.images.count(), 1);
        auto* graph = static_cast<GraphObject*>(b.objectAt(6));
        QCOMPARE(graph->parameters().front().value, 2.5);
        QCOMPARE(graph->evaluate(0, 2.0), 10.0);
    }

    void unusedImagesAreNotSaved()
    {
        DocumentContents c = sampleContents();
        QImage extra(10, 10, QImage::Format_RGB32);
        extra.fill(Qt::blue);
        c.images.addImage(extra);
        QCOMPARE(c.images.count(), 2);
        DocumentContents loaded;
        QVERIFY(ProjectSerializer::deserialize(ProjectSerializer::serialize(c), &loaded, nullptr));
        QCOMPARE(loaded.images.count(), 1);
    }

    void corruptedFileIsRejected()
    {
        QByteArray bytes = ProjectSerializer::serialize(sampleContents());
        QByteArray truncated = bytes.left(bytes.size() / 2);
        DocumentContents out;
        QString error;
        QVERIFY(!ProjectSerializer::deserialize(truncated, &out, &error));
        QVERIFY(!error.isEmpty());
        QByteArray flipped = bytes;
        flipped[flipped.size() - 10] = static_cast<char>(flipped[flipped.size() - 10] ^ 0x5A);
        QVERIFY(!ProjectSerializer::deserialize(flipped, &out, &error));
        QVERIFY(!ProjectSerializer::deserialize(QByteArray("not a lesson"), &out, &error));
    }

    void migrationFromVersionZero()
    {
        // Pre-release documents stored page content under "items".
        QJsonObject stroke = StrokeObject::fromPagePoints({{QPointF(0, 0), 1.0f}, {QPointF(10, 10), 1.0f}}, InkStyle())->toJson();
        QJsonObject page;
        page.insert(QStringLiteral("id"), QStringLiteral("{5b2c9d5c-8a52-4f1e-9a53-3c1f8a1c0a01}"));
        page.insert(QStringLiteral("items"), QJsonArray{stroke});
        QJsonObject root;
        root.insert(QStringLiteral("format"), QStringLiteral("classboard"));
        root.insert(QStringLiteral("pages"), QJsonArray{page});
        Container container;
        container.add(QStringLiteral("document.json"), QJsonDocument(root).toJson(), true);
        DocumentContents out;
        QString error;
        QVERIFY2(ProjectSerializer::deserialize(container.write(), &out, &error), qPrintable(error));
        QCOMPARE(out.pages.front()->objectCount(), 1);
    }

    void unknownObjectsAreSkipped()
    {
        QJsonObject future;
        future.insert(QStringLiteral("type"), QStringLiteral("hologram"));
        QJsonObject page;
        page.insert(QStringLiteral("objects"), QJsonArray{future});
        QJsonObject root;
        root.insert(QStringLiteral("format"), QStringLiteral("classboard"));
        root.insert(QStringLiteral("formatVersion"), 99);
        root.insert(QStringLiteral("pages"), QJsonArray{page});
        Container container;
        container.add(QStringLiteral("document.json"), QJsonDocument(root).toJson(), true);
        DocumentContents out;
        int skipped = 0;
        QVERIFY(ProjectSerializer::deserialize(container.write(), &out, nullptr, &skipped));
        QCOMPARE(skipped, 1);
    }

    void atomicSaveAndLoad()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("lesson.classboard"));
        QString error;
        QVERIFY2(ProjectSerializer::save(path, sampleContents(), &error), qPrintable(error));
        DocumentContents loaded;
        QVERIFY2(ProjectSerializer::load(path, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.pages.size(), size_t(2));
        // No temporary files are left behind.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    void autosaveAndRecovery()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(AutosaveManager::recoveryDirectory()).removeRecursively();
        RecoveryInfo info;
        {
            Document doc;
            AutosaveManager autosave(doc);
            doc.setContents(sampleContents());
            doc.commands().setCleanIndex(-1);
            QVERIFY(autosave.saveNow());
            // While this session runs, its copy is not offered for recovery.
            QVERIFY(AutosaveManager::findRecoverable().isEmpty());
            // Simulate a crash: the lock is released without discarding the recovery copy.
        }
        const QVector<RecoveryInfo> found = AutosaveManager::findRecoverable();
        QCOMPARE(found.size(), 1);
        DocumentContents restored;
        QString error;
        QVERIFY2(AutosaveManager::restore(found.first(), &restored, &error), qPrintable(error));
        QCOMPARE(restored.pages.size(), size_t(2));
        AutosaveManager::discardRecovery(found.first());
        QVERIFY(AutosaveManager::findRecoverable().isEmpty());
    }
};

QTEST_MAIN(TestSerialization)
#include "tst_serialization.moc"
