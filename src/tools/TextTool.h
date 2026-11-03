#pragma once

#include "document/TextObject.h"
#include "tools/Tool.h"

#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>

#include <memory>



namespace cb {

/// Creates and edits text boxes directly on the board with an inline editor (no dialog).
class TextTool final : public QObject, public Tool
{
    Q_OBJECT
public:
    explicit TextTool(ToolHost& host);
    ~TextTool() override;

    ToolId id() const override { return ToolId::Text; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void paintViewOverlay(QPainter& painter) const override;
    void pageChanged() override;
    QCursor cursor() const override { return QCursor(Qt::IBeamCursor); }

    /// Starts editing an existing text object (e.g. after a double tap).
    void editObject(const ObjectId& id);
    bool isEditing() const { return m_editor != nullptr; }
    /// Applies formatting to the text being edited. Returns false when not editing.
    bool applyFormat(const TextFormat& format);
    /// Finishes editing, committing the change as one undo step.
    void commit();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void beginEdit(std::unique_ptr<TextObject> object, bool isNew);
    void updateEditorGeometry() const;
    void cancel();

    QPointer<QPlainTextEdit> m_editor;
    std::unique_ptr<TextObject> m_object;  ///< working copy (new object or clone of the edited one)
    std::unique_ptr<TextObject> m_original;
    bool m_isNew = false;
    PageId m_pageId;
    int m_pointer = -1;
    QPointF m_press;
};

} // namespace cb
