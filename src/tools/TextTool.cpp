#include "tools/TextTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace cb {

TextTool::TextTool(ToolHost& host)
    : QObject(nullptr)
    , Tool(host)
{
}

TextTool::~TextTool()
{
    if (m_editor)
        delete m_editor.data();
}

void TextTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1)
        return;
    m_pointer = e.pointerId;
    m_press = e.pagePos;
}

void TextTool::pointerMove(const PointerEvent& e)
{
    Q_UNUSED(e);
}

void TextTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId == m_pointer)
        m_pointer = -1;
}

void TextTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    if (m_editor) {
        commit();
        return;
    }
    Page* page = host().page();
    if (!page)
        return;
    const qreal tol = host().viewToPageLength(host().theme().dp(6));
    DocumentObject* hit = page->topmostAt(e.pagePos, tol);
    if (hit && hit->type() == ObjectType::Text) {
        editObject(hit->id());
        return;
    }
    const TextFormat format = host().settings().textFormat();
    auto text = TextObject::create(QString(), e.pagePos - QPointF(0, format.pixelSize * 0.6), format, 60);
    beginEdit(std::move(text), true);
}

void TextTool::editObject(const ObjectId& id)
{
    if (m_editor)
        commit();
    Page* page = host().page();
    DocumentObject* o = page ? page->object(id) : nullptr;
    if (!o || o->type() != ObjectType::Text)
        return;
    auto copy = std::unique_ptr<TextObject>(static_cast<TextObject*>(o->clone().release()));
    beginEdit(std::move(copy), false);
}

void TextTool::beginEdit(std::unique_ptr<TextObject> object, bool isNew)
{
    Page* page = host().page();
    if (!page)
        return;
    m_pageId = page->id();
    m_isNew = isNew;
    m_object = std::move(object);
    m_original.reset();
    if (!isNew) {
        m_original.reset(static_cast<TextObject*>(m_object->clone().release()));
        host().setObjectHidden(m_object->id(), true);
    }
    const Theme& t = host().theme();
    auto* editor = new QPlainTextEdit(host().widget());
    editor->setFrameShape(QFrame::NoFrame);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->document()->setDocumentMargin(0);
    editor->setStyleSheet(QStringLiteral("QPlainTextEdit { background: rgba(0,0,0,50); border: %1px dashed %2;"
                                         " border-radius: 4px; padding: 0px; color: %3; }")
                              .arg(qMax(1, t.dpi(2)))
                              .arg(t.color(ThemeColor::Accent).name(), m_object->format().color.name()));
    editor->setPlainText(m_object->text());
    editor->moveCursor(QTextCursor::End);
    editor->installEventFilter(this);
    m_editor = editor;
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_editor || !m_object)
            return;
        m_object->setText(m_editor->toPlainText());
        if (m_isNew)
            m_object->setBoxWidth(std::max(60.0, TextObject::naturalWidth(m_object->text(), m_object->format()) + 12.0));
        updateEditorGeometry();
    });
    updateEditorGeometry();
    editor->show();
    editor->setFocus(Qt::OtherFocusReason);
}

void TextTool::updateEditorGeometry() const
{
    if (!m_editor || !m_object)
        return;
    const ViewTransform& view = host().view();
    const qreal zoom = view.zoom();
    QFont f = m_object->format().font();
    f.setPixelSize(std::max(4, static_cast<int>(std::lround(m_object->format().pixelSize * zoom))));
    f.setHintingPreference(QFont::PreferDefaultHinting);
    if (m_editor->font() != f)
        m_editor->setFont(f);
    const QRectF local = m_object->localBounds();
    // Keep the top-left corner fixed while the box grows.
    const QPointF topLeftPage = m_object->position() + local.topLeft();
    const QPointF topLeft = view.pageToView(topLeftPage);
    const int border = host().theme().dpi(2) + 2;
    const int w = static_cast<int>(std::ceil(local.width() * zoom)) + 2 * border + 6;
    const int h = static_cast<int>(std::ceil(local.height() * zoom)) + 2 * border + 4;
    m_editor->setGeometry(QRect(topLeft.toPoint() - QPoint(border, border), QSize(w, h)));
}

bool TextTool::applyFormat(const TextFormat& format)
{
    if (!m_editor || !m_object)
        return false;
    m_object->setFormat(format);
    if (m_isNew)
        m_object->setBoxWidth(std::max(60.0, TextObject::naturalWidth(m_object->text(), format) + 12.0));
    m_editor->setStyleSheet(m_editor->styleSheet().replace(QRegularExpression(QStringLiteral("color: #[0-9a-fA-F]{6}")),
                                                         QStringLiteral("color: %1").arg(format.color.name())));
    updateEditorGeometry();
    return true;
}

void TextTool::commit()
{
    if (!m_editor || !m_object)
        return;
    const QString text = m_editor->toPlainText();
    m_editor->removeEventFilter(this);
    m_editor->deleteLater();
    m_editor = nullptr;
    Document& doc = host().document();
    const PageId pageId = m_pageId;
    if (m_isNew) {
        if (!text.trimmed().isEmpty()) {
            m_object->setText(text);
            std::vector<ObjectPtr> objects;
            objects.push_back(std::move(m_object));
            doc.commands().push(std::make_unique<AddObjectsCommand>(pageId, std::move(objects), QObject::tr("Add text")));
        }
    } else {
        host().setObjectHidden(m_object->id(), false);
        const bool textChanged = m_original && text != m_original->text();
        const bool formatChanged = m_original && (m_object->format().pixelSize != m_original->format().pixelSize
                                                  || m_object->format().color != m_original->format().color
                                                  || m_object->format().bold != m_original->format().bold
                                                  || m_object->format().italic != m_original->format().italic
                                                  || m_object->format().underline != m_original->format().underline
                                                  || m_object->format().alignment != m_original->format().alignment
                                                  || m_object->format().family != m_original->format().family);
        if (text.trimmed().isEmpty()) {
            doc.commands().push(std::make_unique<RemoveObjectsCommand>(pageId, std::vector<ObjectId>{m_object->id()},
                                                                       QObject::tr("Delete text")));
        } else if (textChanged || formatChanged) {
            m_object->setText(text);
            auto cmd = std::make_unique<ModifyObjectsCommand>(pageId, QObject::tr("Edit text"));
            cmd->add(std::move(m_original), std::move(m_object));
            doc.commands().push(std::move(cmd));
        }
    }
    m_object.reset();
    m_original.reset();
    host().updateOverlayAll();
}

void TextTool::cancel()
{
    if (m_editor) {
        m_editor->removeEventFilter(this);
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    if (m_object && !m_isNew)
        host().setObjectHidden(m_object->id(), false);
    m_object.reset();
    m_original.reset();
}

bool TextTool::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_editor && event->type() == QEvent::ShortcutOverride) {
        // Keys the editor handles itself must not trigger window shortcuts (Escape, Delete ...).
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            event->accept();
            return true;
        }
    }
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape
            || ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && (ke->modifiers() & Qt::ControlModifier))) {
            commit();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void TextTool::deactivate()
{
    commit();
    m_pointer = -1;
}

void TextTool::pageChanged()
{
    commit();
    m_pointer = -1;
}

void TextTool::paintViewOverlay(QPainter& painter) const
{
    Q_UNUSED(painter);
    // Keep the inline editor glued to its text box while zooming or panning.
    updateEditorGeometry();
}

} // namespace cb
