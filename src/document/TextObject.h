#pragma once

#include "document/DocumentObject.h"

#include <QFont>

namespace cb {

/// Formatting applied to a whole text object.
struct TextFormat
{
    QString family;
    int pixelSize = 40;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    QColor color = QColor(245, 245, 240);
    Qt::Alignment alignment = Qt::AlignLeft;

    QFont font() const;
};

/// Editable text box. Width is fixed by the user; height follows the wrapped content.
class TextObject final : public DocumentObject
{
public:
    TextObject();
    static std::unique_ptr<TextObject> create(const QString& text, const QPointF& topLeft, const TextFormat& format,
                                              qreal width = 0);

    const QString& text() const { return m_text; }
    void setText(const QString& text);
    const TextFormat& format() const { return m_format; }
    void setFormat(const TextFormat& format);
    qreal boxWidth() const { return m_width; }
    void setBoxWidth(qreal width);

    /// Natural width of the text without wrapping (used for new text boxes).
    static qreal naturalWidth(const QString& text, const TextFormat& format);

    QRectF localBounds() const override;
    qreal outlineMargin() const override { return 4.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_format.color; }
    bool isEditable() const override { return true; }
    std::unique_ptr<DocumentObject> clone() const override;

protected:
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    TextObject(const TextObject&) = default;
    qreal contentHeight() const;
    int flags() const;

    QString m_text;
    TextFormat m_format;
    qreal m_width = 400.0;
    mutable qreal m_heightCache = -1.0;
};

} // namespace cb
