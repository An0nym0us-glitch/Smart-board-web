#pragma once

#include "core/Id.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class QMimeData;

namespace cb {

class Document;
class SelectionModel;

/// Selection-level editing shared by keyboard shortcuts, the selection action bar and popovers.
/// Every operation is a single undoable document command.
class EditOperations
{
public:
    static constexpr const char* kMimeType = "application/x-classboard-objects";

    EditOperations(Document& doc, SelectionModel& selection);

    bool hasSelection() const;
    void deleteSelection();
    void duplicateSelection();
    void copySelection();
    void cutSelection();
    /// Pastes ClassBoard objects, an image or plain text. pageCenter is used when the clipboard
    /// content has no position of its own.
    bool paste(const QPointF& pageCenter);
    bool canPaste() const;
    void selectAll();
    void bringToFront();
    void sendToBack();
    void setSelectionColor(const QColor& color);

    /// Serialises objects (and referenced images) into mime data.
    QMimeData* createMimeData(const QVector<ObjectId>& ids) const;
    /// Inserts objects from mime data; returns the new ids.
    QVector<ObjectId> insertMimeData(const QMimeData* mime, const QPointF& pageCenter, bool offsetIfSamePlace);

private:
    Document& m_doc;
    SelectionModel& m_selection;
    int m_pasteCount = 0;
};

} // namespace cb
