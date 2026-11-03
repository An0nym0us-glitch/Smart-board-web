#include "ui/TableCellEditor.h"

#include "canvas/CanvasWidget.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "graph/TableObject.h"
#include "ui/UiContext.h"

#include <QKeyEvent>
#include <QLineEdit>

#include <cmath>

namespace cb {

TableCellEditor::TableCellEditor(const UiContext& ui, CanvasWidget& canvas, Document& doc, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_canvas(canvas)
    , m_doc(doc)
{
    connect(&canvas, &CanvasWidget::viewChanged, this, &TableCellEditor::reposition);
    connect(&doc, &Document::currentPageChanged, this, [this]() { commit(); });
}

TableCellEditor::~TableCellEditor()
{
    if (m_editor)
        delete m_editor.data();
}

void TableCellEditor::edit(const ObjectId& table, int row, int column)
{
    commit();
    Page* page = m_doc.currentPage();
    DocumentObject* o = page ? page->object(table) : nullptr;
    if (!o || o->type() != ObjectType::Table)
        return;
    m_table = table;
    m_page = page->id();
    auto* editor = new QLineEdit(&m_canvas);
    editor->setAlignment(Qt::AlignCenter);
    editor->setStyleSheet(QStringLiteral("QLineEdit { background: rgba(20,26,24,235); border: 2px solid %1; border-radius: 4px;"
                                         " color: white; padding: 0px; }")
                              .arg(m_ui.theme.color(ThemeColor::Accent).name()));
    editor->installEventFilter(this);
    m_editor = editor;
    moveTo(row, column);
    editor->show();
    editor->setFocus();
}

void TableCellEditor::moveTo(int row, int column)
{
    Page* page = m_doc.pageById(m_page);
    auto* table = page ? static_cast<TableObject*>(page->object(m_table)) : nullptr;
    if (!table || !m_editor)
        return;
    m_row = std::clamp(row, 0, table->rowCount() - 1);
    m_col = std::clamp(column, 0, table->columnCount() - 1);
    m_editor->setText(table->cell(m_row, m_col));
    m_editor->selectAll();
    reposition();
}

void TableCellEditor::reposition()
{
    if (!m_editor)
        return;
    Page* page = m_doc.pageById(m_page);
    auto* table = page ? static_cast<TableObject*>(page->object(m_table)) : nullptr;
    if (!table)
        return;
    const QRectF cell = table->cellRect(m_row, m_col);
    const QRectF pageRect = table->transform().mapRect(cell);
    m_editor->setGeometry(m_canvas.pageRectToWidget(pageRect));
    QFont f = m_ui.theme.font();
    f.setPixelSize(std::max(8, static_cast<int>(std::lround(table->fontSize() * m_canvas.zoom()))));
    m_editor->setFont(f);
}

void TableCellEditor::commit()
{
    if (!m_editor)
        return;
    const QString text = m_editor->text();
    m_editor->removeEventFilter(this);
    m_editor->deleteLater();
    m_editor = nullptr;
    Page* page = m_doc.pageById(m_page);
    DocumentObject* o = page ? page->object(m_table) : nullptr;
    if (!o || o->type() != ObjectType::Table)
        return;
    if (static_cast<TableObject*>(o)->cell(m_row, m_col) == text)
        return;
    auto after = o->clone();
    static_cast<TableObject*>(after.get())->setCell(m_row, m_col, text);
    auto cmd = std::make_unique<ModifyObjectsCommand>(m_page, tr("Edit cell"));
    cmd->add(o->clone(), std::move(after));
    m_doc.commands().push(std::move(cmd));
}

bool TableCellEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_editor) {
        if (event->type() == QEvent::ShortcutOverride) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Escape || key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Return
                || key == Qt::Key_Enter || key == Qt::Key_Delete || key == Qt::Key_Backspace) {
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            const int row = m_row;
            const int col = m_col;
            const ObjectId table = m_table;
            switch (ke->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                commit();
                edit(table, row + 1, col);
                return true;
            case Qt::Key_Tab:
                commit();
                edit(table, row, col + 1);
                return true;
            case Qt::Key_Backtab:
                commit();
                edit(table, row, col - 1);
                return true;
            case Qt::Key_Escape:
                commit();
                return true;
            default:
                break;
            }
        } else if (event->type() == QEvent::FocusOut) {
            commit();
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace cb
