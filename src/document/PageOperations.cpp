#include "document/PageOperations.h"

#include "document/Commands.h"
#include "document/Document.h"

#include <QCoreApplication>

namespace cb::pageops {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("PageOperations", s); }

QVector<int> resolve(const Document& doc, const QVector<int>& pages)
{
    QVector<int> out;
    if (pages.isEmpty()) {
        for (int i = 0; i < doc.pageCount(); ++i)
            out.push_back(i);
        return out;
    }
    for (int i : pages)
        if (i >= 0 && i < doc.pageCount() && !out.contains(i))
            out.push_back(i);
    return out;
}
} // namespace

void newPage(Document& doc, int afterIndex)
{
    const int index = std::clamp(afterIndex + 1, 0, doc.pageCount());
    doc.commands().push(std::make_unique<InsertPageCommand>(index, doc.createPage(), tr("New page")));
}

bool duplicatePage(Document& doc, int index)
{
    Page* page = doc.page(index);
    if (!page)
        return false;
    doc.commands().push(std::make_unique<InsertPageCommand>(index + 1, page->clone(true), tr("Duplicate page")));
    return true;
}

bool clearPage(Document& doc, int index)
{
    Page* page = doc.page(index);
    if (!page || page->objectCount() == 0)
        return false;
    std::vector<ObjectId> ids;
    ids.reserve(static_cast<size_t>(page->objectCount()));
    for (const auto& o : page->objects())
        ids.push_back(o->id());
    doc.commands().push(std::make_unique<RemoveObjectsCommand>(page->id(), std::move(ids), tr("Clear page")));
    return true;
}

bool deletePage(Document& doc, int index)
{
    if (doc.pageCount() <= 1 || !doc.page(index))
        return false;
    doc.commands().push(std::make_unique<RemovePageCommand>(index));
    return true;
}

bool setPageSize(Document& doc, const QVector<int>& pages, const QSizeF& size)
{
    if (size.width() < 100 || size.height() < 100)
        return false;
    const QVector<int> targets = resolve(doc, pages);
    auto macro = std::make_unique<CompositeCommand>(tr("Change page size"));
    for (int i : targets) {
        Page* page = doc.page(i);
        if (!page || page->size() == size)
            continue;
        auto cmd = std::make_unique<SetPageSizeCommand>(page->id(), size, QString());
        cmd->redo(doc);
        macro->add(std::move(cmd));
    }
    if (macro->isEmpty())
        return false;
    doc.commands().pushApplied(std::move(macro));
    return true;
}

TemplateSpec withBackgroundColor(const TemplateSpec& spec, const QColor& color)
{
    TemplateSpec out = spec;
    out.background = color;
    out.background.setAlpha(255);
    out.id = QStringLiteral("color-") + color.name(QColor::HexRgb).mid(1);
    out.name = tr("Custom colour");
    const bool dark = out.isDark();
    out.lineColor = dark ? QColor(255, 255, 255, 40) : QColor(0, 40, 80, 45);
    out.accentColor = dark ? QColor(255, 255, 255, 90) : QColor(0, 40, 80, 100);
    return out;
}

bool setBackgroundColor(Document& doc, const QVector<int>& pages, const QColor& color)
{
    if (!color.isValid())
        return false;
    const QVector<int> targets = resolve(doc, pages);
    auto macro = std::make_unique<CompositeCommand>(tr("Change background colour"));
    for (int i : targets) {
        Page* page = doc.page(i);
        if (!page)
            continue;
        const TemplateSpec spec = withBackgroundColor(page->background(), color);
        if (spec == page->background())
            continue;
        auto cmd = std::make_unique<ModifyPageCommand>(page->id(), page->name(), spec, QString());
        cmd->redo(doc);
        macro->add(std::move(cmd));
    }
    if (macro->isEmpty())
        return false;
    doc.commands().pushApplied(std::move(macro));
    return true;
}

} // namespace cb::pageops
