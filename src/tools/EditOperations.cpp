#include "tools/EditOperations.h"

#include "canvas/PageRenderer.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ObjectFactory.h"
#include "tools/SelectionModel.h"

#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>

#include <algorithm>

namespace cb {

EditOperations::EditOperations(Document& doc, SelectionModel& selection)
    : m_doc(doc)
    , m_selection(selection)
{
}

bool EditOperations::hasSelection() const
{
    return !m_selection.isEmpty() && m_doc.currentPage();
}

void EditOperations::deleteSelection()
{
    Page* page = m_doc.currentPage();
    if (!page || m_selection.isEmpty())
        return;
    std::vector<ObjectId> ids(m_selection.ids().begin(), m_selection.ids().end());
    m_selection.clear();
    m_doc.commands().push(std::make_unique<RemoveObjectsCommand>(page->id(), std::move(ids), QObject::tr("Delete")));
}

void EditOperations::duplicateSelection()
{
    Page* page = m_doc.currentPage();
    if (!page || m_selection.isEmpty())
        return;
    std::vector<ObjectPtr> copies;
    QVector<ObjectId> newIds;
    for (const ObjectId& id : m_selection.ids()) {
        DocumentObject* o = page->object(id);
        if (!o)
            continue;
        ObjectPtr c = o->clone();
        c->setId(newId());
        c->setPosition(o->position() + QPointF(32, 32));
        newIds.push_back(c->id());
        copies.push_back(std::move(c));
    }
    if (copies.empty())
        return;
    m_doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(copies), QObject::tr("Duplicate")));
    m_selection.set(newIds);
}

QMimeData* EditOperations::createMimeData(const QVector<ObjectId>& ids) const
{
    Page* page = m_doc.currentPage();
    auto* mime = new QMimeData();
    if (!page)
        return mime;
    QJsonArray objects;
    QSet<QString> images;
    QRectF bounds;
    auto tempPage = std::make_unique<Page>();
    tempPage->setBackground(page->background());
    for (const ObjectId& id : ids) {
        DocumentObject* o = page->object(id);
        if (!o)
            continue;
        objects.append(o->toJson());
        o->collectImageKeys(images);
        bounds = bounds.isNull() ? o->sceneBounds() : bounds.united(o->sceneBounds());
        tempPage->insertObject(tempPage->objectCount(), o->clone());
    }
    QJsonObject imageData;
    for (const QString& key : images) {
        QJsonObject img;
        img.insert(QStringLiteral("data"), QString::fromLatin1(m_doc.images().encodedData(key).toBase64()));
        imageData.insert(key, img);
    }
    QJsonObject root;
    root.insert(QStringLiteral("objects"), objects);
    root.insert(QStringLiteral("images"), imageData);
    root.insert(QStringLiteral("page"), idToString(page->id()));
    mime->setData(QString::fromLatin1(kMimeType), QJsonDocument(root).toJson(QJsonDocument::Compact));

    // A rendered picture for other applications.
    if (!bounds.isNull()) {
        const QRectF area = geom::inflated(bounds, 12);
        const qreal scale = std::min(2.0, 4096.0 / std::max(area.width(), area.height()));
        const QSize size(std::max(1, qRound(area.width() * scale)), std::max(1, qRound(area.height() * scale)));
        mime->setImageData(PageRenderer::renderToImage(*tempPage, area, size, m_doc.images(), m_doc.coordinates()));
    }
    return mime;
}

void EditOperations::copySelection()
{
    if (!hasSelection())
        return;
    m_pasteCount = 0;
    QApplication::clipboard()->setMimeData(createMimeData(m_selection.ids()));
}

void EditOperations::cutSelection()
{
    copySelection();
    deleteSelection();
}

bool EditOperations::canPaste() const
{
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    return mime && (mime->hasFormat(QString::fromLatin1(kMimeType)) || mime->hasImage() || mime->hasText());
}

QVector<ObjectId> EditOperations::insertMimeData(const QMimeData* mime, const QPointF& pageCenter, bool offsetIfSamePlace)
{
    QVector<ObjectId> ids;
    Page* page = m_doc.currentPage();
    if (!page || !mime)
        return ids;
    std::vector<ObjectPtr> objects;
    QString text = QObject::tr("Paste");

    if (mime->hasFormat(QString::fromLatin1(kMimeType))) {
        const QJsonObject root = QJsonDocument::fromJson(mime->data(QString::fromLatin1(kMimeType))).object();
        const QJsonObject images = root.value(QStringLiteral("images")).toObject();
        for (auto it = images.constBegin(); it != images.constEnd(); ++it) {
            const QByteArray bytes = QByteArray::fromBase64(it.value().toObject().value(QStringLiteral("data")).toString().toLatin1());
            m_doc.images().addEncoded(bytes);
        }
        const bool samePage = root.value(QStringLiteral("page")).toString() == idToString(page->id());
        QPointF offset;
        if (samePage && offsetIfSamePlace) {
            ++m_pasteCount;
            offset = QPointF(32, 32) * m_pasteCount;
        }
        QRectF bounds;
        for (const QJsonValue& v : root.value(QStringLiteral("objects")).toArray()) {
            ObjectPtr o = ObjectFactory::fromJson(v.toObject());
            if (!o)
                continue;
            o->setId(newId());
            bounds = bounds.isNull() ? o->sceneBounds() : bounds.united(o->sceneBounds());
            objects.push_back(std::move(o));
        }
        if (!samePage && !bounds.isNull())
            offset = pageCenter - bounds.center();
        for (auto& o : objects)
            o->setPosition(o->position() + offset);
    } else if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        const QString key = m_doc.images().addImage(image);
        if (!key.isEmpty()) {
            if (ObjectPtr o = ObjectFactory::createImage(key, image.size(), pageCenter))
                objects.push_back(std::move(o));
        }
        text = QObject::tr("Paste image");
    } else if (mime->hasText() && !mime->text().trimmed().isEmpty()) {
        if (ObjectPtr o = ObjectFactory::createText(mime->text().trimmed(), pageCenter))
            objects.push_back(std::move(o));
        text = QObject::tr("Paste text");
    }
    if (objects.empty())
        return ids;
    for (const auto& o : objects)
        ids.push_back(o->id());
    m_doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), text));
    return ids;
}

bool EditOperations::paste(const QPointF& pageCenter)
{
    const QVector<ObjectId> ids = insertMimeData(QApplication::clipboard()->mimeData(), pageCenter, true);
    if (ids.isEmpty())
        return false;
    m_selection.set(ids);
    return true;
}

void EditOperations::selectAll()
{
    Page* page = m_doc.currentPage();
    if (!page)
        return;
    QVector<ObjectId> ids;
    for (const auto& o : page->objects())
        ids.push_back(o->id());
    m_selection.set(ids);
}

void EditOperations::bringToFront()
{
    Page* page = m_doc.currentPage();
    if (!page || m_selection.isEmpty())
        return;
    // Preserve the relative order of the selected objects.
    QVector<QPair<int, ObjectId>> ordered;
    for (const ObjectId& id : m_selection.ids()) {
        const int i = page->indexOf(id);
        if (i >= 0)
            ordered.push_back({i, id});
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto macro = std::make_unique<CompositeCommand>(QObject::tr("Bring to front"));
    const int last = page->objectCount() - 1;
    for (const auto& entry : ordered) {
        auto cmd = std::make_unique<ReorderObjectCommand>(page->id(), entry.second, page->indexOf(entry.second), last);
        cmd->redo(m_doc);
        macro->add(std::move(cmd));
    }
    if (!macro->isEmpty())
        m_doc.commands().pushApplied(std::move(macro));
}

void EditOperations::sendToBack()
{
    Page* page = m_doc.currentPage();
    if (!page || m_selection.isEmpty())
        return;
    QVector<QPair<int, ObjectId>> ordered;
    for (const ObjectId& id : m_selection.ids()) {
        const int i = page->indexOf(id);
        if (i >= 0)
            ordered.push_back({i, id});
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    auto macro = std::make_unique<CompositeCommand>(QObject::tr("Send to back"));
    for (const auto& entry : ordered) {
        auto cmd = std::make_unique<ReorderObjectCommand>(page->id(), entry.second, page->indexOf(entry.second), 0);
        cmd->redo(m_doc);
        macro->add(std::move(cmd));
    }
    if (!macro->isEmpty())
        m_doc.commands().pushApplied(std::move(macro));
}

void EditOperations::setSelectionColor(const QColor& color)
{
    Page* page = m_doc.currentPage();
    if (!page || m_selection.isEmpty())
        return;
    auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), QObject::tr("Change colour"));
    for (const ObjectId& id : m_selection.ids()) {
        DocumentObject* o = page->object(id);
        if (!o)
            continue;
        ObjectPtr after = o->clone();
        if (after->setColor(color))
            cmd->add(o->clone(), std::move(after));
    }
    if (!cmd->isEmpty())
        m_doc.commands().push(std::move(cmd));
}

} // namespace cb
