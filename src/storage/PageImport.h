#pragma once

#include "storage/PdfImporter.h"

#include <QString>

namespace cb {

class Document;

/// How imported pages are sized.
enum class ImportSizing {
    Physical,   ///< keep the document's physical page size (PDF worksheets: A4 stays A4)
    BoardSized, ///< long side as wide as the classroom board, same aspect ratio (slides)
};

/// Inserts imported pages after afterIndex as one undoable command. Each source page becomes a
/// board page whose logical size has exactly the source aspect ratio, with the page image as an
/// object filling the page (nothing cropped or stretched) on a white background. Returns the
/// number of pages added.
int insertImportedPages(Document& doc, int afterIndex, const QVector<ImportedPage>& pages, const QString& baseName,
                        ImportSizing sizing, const QString& commandText);

} // namespace cb
