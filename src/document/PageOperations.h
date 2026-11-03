#pragma once

#include <QColor>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace cb {

class Document;
struct TemplateSpec;

/// Page-level actions shared by the ribbon, the page navigator and the eraser panel. Each action
/// is one undoable command. Page indices are 0-based.
namespace pageops {

/// Inserts a blank page (default size and background) after afterIndex and shows it.
void newPage(Document& doc, int afterIndex);
/// Inserts a copy of the page after it. Returns false for an invalid index.
bool duplicatePage(Document& doc, int index);
/// Removes every object from the page but keeps the page, its size and its background.
/// Returns false if the page is invalid or already empty.
bool clearPage(Document& doc, int index);
/// Removes the page. Returns false for an invalid index or if it is the only page.
bool deletePage(Document& doc, int index);

/// Changes the logical size of the given pages (all pages if empty).
bool setPageSize(Document& doc, const QVector<int>& pages, const QSizeF& size);
/// Sets a plain background colour on the given pages (all if empty), keeping the page pattern
/// (grid, lines ...) with line colours adjusted for contrast.
bool setBackgroundColor(Document& doc, const QVector<int>& pages, const QColor& color);
/// Returns spec with a new background colour and matching line colours.
TemplateSpec withBackgroundColor(const TemplateSpec& spec, const QColor& color);

} // namespace pageops
} // namespace cb
