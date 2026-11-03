#include "storage/PageImport.h"

#include "document/Commands.h"
#include "document/Document.h"
#include "document/ImageObject.h"
#include "document/PageOperations.h"
#include "document/PageSize.h"

#include <algorithm>

namespace cb {

int insertImportedPages(Document& doc, int afterIndex, const QVector<ImportedPage>& pages, const QString& baseName,
                        ImportSizing sizing, const QString& commandText)
{
    auto macro = std::make_unique<CompositeCommand>(commandText);
    int index = std::clamp(afterIndex + 1, 0, doc.pageCount());
    int added = 0;
    for (int i = 0; i < pages.size(); ++i) {
        const ImportedPage& source = pages[i];
        if (source.pixelSize.isEmpty())
            continue;
        const QString key = doc.images().addEncoded(source.encoded);
        if (key.isEmpty())
            continue;
        // Page size: physical (mm -> document units) or board sized; the height always follows the
        // exact pixel aspect ratio of the rendered page so the image fills the page 1:1.
        const qreal aspect = qreal(source.pixelSize.height()) / source.pixelSize.width();
        QSizeF size;
        if (sizing == ImportSizing::Physical && source.sizeMm.width() > 1.0) {
            const qreal width = std::clamp(source.sizeMm.width() / 10.0, 5.0, 500.0) * pagesize::kUnitsPerCm;
            size = QSizeF(width, width * aspect);
        } else {
            size = pagesize::forAspect(source.pixelSize.width(), source.pixelSize.height());
        }
        PagePtr page = doc.createPage();
        page->setSize(size);
        page->setBackground(pageops::withBackgroundColor(page->background(), Qt::white));
        page->setName(QStringLiteral("%1 %2").arg(baseName).arg(i + 1));
        page->insertObject(0, ImageObject::create(key, size, page->frameRect().center()));
        auto cmd = std::make_unique<InsertPageCommand>(index++, std::move(page));
        cmd->redo(doc);
        macro->add(std::move(cmd));
        ++added;
    }
    if (added > 0)
        doc.commands().pushApplied(std::move(macro));
    return added;
}

} // namespace cb
