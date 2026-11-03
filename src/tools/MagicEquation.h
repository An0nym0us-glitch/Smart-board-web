#pragma once

#include "ai/Recognition.h"
#include "core/Id.h"

#include <QString>
#include <QVector>

namespace cb {

class Document;
class Page;

/// Document side of the Magic Equation Maker. Recognition never changes the document; only
/// acceptEquation() does, as one undoable step that replaces the selected handwriting.
namespace magic {

/// True if ids are one or more handwriting strokes on the page (and nothing else).
bool isHandwriting(const Page& page, const QVector<ObjectId>& ids);
/// The handwriting as recogniser input (page coordinates, in z-order).
InkSample collectInk(const Page& page, const QVector<ObjectId>& ids);
/// Replaces the strokes by an editable equation object at the same place, sized like the
/// handwriting and in the ink colour. Returns the new equation's id (null on failure).
ObjectId acceptEquation(Document& doc, const Page& page, const QVector<ObjectId>& strokes, const QString& latex);

} // namespace magic
} // namespace cb
