#pragma once

#include "core/Id.h"

#include <QLineEdit>
#include <QObject>
#include <QPointer>

namespace cb {

class CanvasWidget;
class Document;
struct UiContext;

/// Inline editor for table cells, placed exactly over the cell on the board.
/// Enter moves down, Tab moves right, Escape or tapping elsewhere finishes.
class TableCellEditor : public QObject
{
    Q_OBJECT
public:
    TableCellEditor(const UiContext& ui, CanvasWidget& canvas, Document& doc, QObject* parent = nullptr);
    ~TableCellEditor() override;

    void edit(const ObjectId& table, int row, int column);
    void commit();
    bool isEditing() const { return m_editor != nullptr; }
    void reposition();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void moveTo(int row, int column);

    const UiContext& m_ui;
    CanvasWidget& m_canvas;
    Document& m_doc;
    QPointer<QLineEdit> m_editor;
    ObjectId m_table;
    PageId m_page;
    int m_row = 0;
    int m_col = 0;
};

} // namespace cb
