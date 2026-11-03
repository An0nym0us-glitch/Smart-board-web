#include "storage/ProjectSerializer.h"

#include "core/JsonUtil.h"
#include "storage/Container.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace cb {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("ProjectSerializer", s); }
const QString kDocumentEntry = QStringLiteral("document.json");
const QString kImagePrefix = QStringLiteral("images/");

QSet<QString> referencedImages(const DocumentContents& c)
{
    QSet<QString> keys;
    for (const auto& page : c.pages) {
        if (!page->background().imageKey.isEmpty())
            keys.insert(page->background().imageKey);
        for (const auto& o : page->objects())
            o->collectImageKeys(keys);
    }
    if (!c.defaultTemplate.imageKey.isEmpty())
        keys.insert(c.defaultTemplate.imageKey);
    return keys;
}
} // namespace

QJsonObject ProjectSerializer::documentToJson(const DocumentContents& c)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("classboard"));
    root.insert(QStringLiteral("formatVersion"), kFormatVersion);
    root.insert(QStringLiteral("application"), QStringLiteral("ClassBoard %1").arg(QCoreApplication::applicationVersion()));

    QJsonObject meta;
    meta.insert(QStringLiteral("title"), c.metadata.title);
    meta.insert(QStringLiteral("author"), c.metadata.author);
    meta.insert(QStringLiteral("created"), c.metadata.created.toString(Qt::ISODate));
    meta.insert(QStringLiteral("modified"), c.metadata.modified.toString(Qt::ISODate));
    root.insert(QStringLiteral("metadata"), meta);

    root.insert(QStringLiteral("coordinates"), c.coordinates.toJson());
    root.insert(QStringLiteral("defaultTemplate"), c.defaultTemplate.toJson());
    root.insert(QStringLiteral("defaultPageSize"), json::fromSize(c.defaultPageSize));
    root.insert(QStringLiteral("currentPage"), c.currentPage);

    QJsonArray pages;
    for (const auto& p : c.pages)
        pages.append(p->toJson());
    root.insert(QStringLiteral("pages"), pages);
    return root;
}

bool ProjectSerializer::documentFromJson(const QJsonObject& root, DocumentContents* out, int* skippedObjects)
{
    const QJsonObject meta = root.value(QStringLiteral("metadata")).toObject();
    out->metadata.title = meta.value(QStringLiteral("title")).toString();
    out->metadata.author = meta.value(QStringLiteral("author")).toString();
    out->metadata.created = QDateTime::fromString(meta.value(QStringLiteral("created")).toString(), Qt::ISODate);
    out->metadata.modified = QDateTime::fromString(meta.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    if (root.contains(QStringLiteral("coordinates")))
        out->coordinates = CoordinateSystem::fromJson(root.value(QStringLiteral("coordinates")).toObject());
    out->defaultTemplate = TemplateSpec::fromJson(root.value(QStringLiteral("defaultTemplate")).toObject());
    const QSizeF pageSize = json::toSize(root.value(QStringLiteral("defaultPageSize")), QSizeF(Page::kDefaultWidth, Page::kDefaultHeight));
    if (pageSize.width() >= 100 && pageSize.height() >= 100)
        out->defaultPageSize = pageSize;
    out->currentPage = root.value(QStringLiteral("currentPage")).toInt(0);
    out->pages.clear();
    int skipped = 0;
    for (const QJsonValue& v : root.value(QStringLiteral("pages")).toArray())
        out->pages.push_back(Page::fromJson(v.toObject(), &skipped));
    if (skippedObjects)
        *skippedObjects = skipped;
    return !out->pages.empty();
}

bool ProjectSerializer::migrate(QJsonObject& root, QString* error)
{
    int version = root.value(QStringLiteral("formatVersion")).toInt(0);
    if (version > kFormatVersion) {
        // Newer file: load what we understand; unknown objects are skipped by the factory.
        return true;
    }
    while (version < kFormatVersion) {
        switch (version) {
        case 0: {
            // Pre-release format: pages stored their content under "items".
            QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
            for (int i = 0; i < pages.size(); ++i) {
                QJsonObject page = pages.at(i).toObject();
                if (page.contains(QStringLiteral("items")) && !page.contains(QStringLiteral("objects"))) {
                    page.insert(QStringLiteral("objects"), page.value(QStringLiteral("items")));
                    page.remove(QStringLiteral("items"));
                }
                pages.replace(i, page);
            }
            root.insert(QStringLiteral("pages"), pages);
            break;
        }
        default:
            if (error)
                *error = tr("Unsupported lesson format version %1.").arg(version);
            return false;
        }
        ++version;
        root.insert(QStringLiteral("formatVersion"), version);
    }
    return true;
}

QByteArray ProjectSerializer::serialize(const DocumentContents& contents)
{
    Container container;
    const QJsonDocument doc(documentToJson(contents));
    container.add(kDocumentEntry, doc.toJson(QJsonDocument::Compact), true);
    const QSet<QString> used = referencedImages(contents);
    for (const QString& key : used) {
        const QByteArray bytes = contents.images.encodedData(key);
        if (!bytes.isEmpty())
            container.add(kImagePrefix + key + QLatin1Char('.') + contents.images.suffix(key), bytes, false);
    }
    return container.write();
}

bool ProjectSerializer::deserialize(const QByteArray& bytes, DocumentContents* out, QString* error, int* skippedObjects)
{
    Container container;
    if (!container.read(bytes, error))
        return false;
    const Container::Entry* docEntry = container.find(kDocumentEntry);
    if (!docEntry) {
        if (error)
            *error = tr("The lesson file does not contain a document.");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(docEntry->data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (error)
            *error = tr("The lesson document is damaged: %1").arg(parseError.errorString());
        return false;
    }
    QJsonObject root = json.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("classboard")) {
        if (error)
            *error = tr("This is not a ClassBoard lesson.");
        return false;
    }
    if (!migrate(root, error))
        return false;

    DocumentContents contents;
    for (const Container::Entry& e : container.entries()) {
        if (!e.name.startsWith(kImagePrefix))
            continue;
        contents.images.addEncoded(e.data);
    }
    if (!documentFromJson(root, &contents, skippedObjects)) {
        if (error)
            *error = tr("The lesson does not contain any pages.");
        return false;
    }
    *out = std::move(contents);
    return true;
}

bool ProjectSerializer::writeFileAtomically(const QString& path, const QByteArray& bytes, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error)
            *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool ProjectSerializer::save(const QString& path, const DocumentContents& contents, QString* error)
{
    return writeFileAtomically(path, serialize(contents), error);
}

bool ProjectSerializer::load(const QString& path, DocumentContents* out, QString* error, int* skippedObjects)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return deserialize(file.readAll(), out, error, skippedObjects);
}

} // namespace cb
