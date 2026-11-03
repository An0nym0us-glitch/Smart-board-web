#include "document/TextObject.h"

#include "core/JsonUtil.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cb {

QFont TextFormat::font() const
{
    QFont f(family.isEmpty() ? QFont().family() : family);
    f.setPixelSize(std::max(6, pixelSize));
    f.setBold(bold);
    f.setItalic(italic);
    f.setUnderline(underline);
    // Unhinted metrics keep the layout identical at every zoom level and in exports.
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

TextObject::TextObject()
    : DocumentObject(ObjectType::Text)
{
}

std::unique_ptr<TextObject> TextObject::create(const QString& text, const QPointF& topLeft, const TextFormat& format,
                                               qreal width)
{
    auto t = std::make_unique<TextObject>();
    t->m_text = text;
    t->m_format = format;
    t->m_width = width > 0 ? width : std::max(120.0, naturalWidth(text, format) + 8.0);
    const qreal h = t->contentHeight();
    t->setPosition(topLeft + QPointF(t->m_width / 2, h / 2));
    return t;
}

qreal TextObject::naturalWidth(const QString& text, const TextFormat& format)
{
    const QFontMetricsF fm(format.font());
    qreal w = 0;
    for (const QString& line : text.split(QLatin1Char('\n')))
        w = std::max(w, fm.horizontalAdvance(line));
    return std::min(w, 1600.0);
}

void TextObject::setText(const QString& text)
{
    m_text = text;
    m_heightCache = -1;
    invalidateBounds();
}

void TextObject::setFormat(const TextFormat& format)
{
    m_format = format;
    m_heightCache = -1;
    invalidateBounds();
}

void TextObject::setBoxWidth(qreal width)
{
    m_width = std::max(20.0, width);
    m_heightCache = -1;
    invalidateBounds();
}

int TextObject::flags() const
{
    return int(m_format.alignment) | Qt::AlignTop | Qt::TextWordWrap;
}

qreal TextObject::contentHeight() const
{
    if (m_heightCache < 0) {
        const QFontMetricsF fm(m_format.font());
        const QString text = m_text.isEmpty() ? QStringLiteral(" ") : m_text;
        const QRectF r = fm.boundingRect(QRectF(0, 0, m_width, 1e6), flags(), text);
        m_heightCache = std::max(fm.height(), r.height());
    }
    return m_heightCache;
}

QRectF TextObject::localBounds() const
{
    const qreal h = contentHeight();
    return QRectF(-m_width / 2, -h / 2, m_width, h);
}

void TextObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    painter.save();
    painter.setFont(m_format.font());
    painter.setPen(m_format.color);
    painter.drawText(localBounds(), flags(), m_text);
    painter.restore();
}

bool TextObject::setColor(const QColor& color)
{
    m_format.color = color;
    return true;
}

std::unique_ptr<DocumentObject> TextObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new TextObject(*this));
}

void TextObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    const QRectF old = localBounds();
    if (hint != ResizeHint::Horizontal && old.height() > 1.0) {
        const qreal factor = newSize.height() / old.height();
        m_format.pixelSize = std::clamp(static_cast<int>(std::lround(m_format.pixelSize * factor)), 8, 800);
    }
    m_width = std::max(20.0, newSize.width());
    m_heightCache = -1;
}

void TextObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("text"), m_text);
    obj.insert(QStringLiteral("width"), m_width);
    if (!m_format.family.isEmpty())
        obj.insert(QStringLiteral("family"), m_format.family);
    obj.insert(QStringLiteral("size"), m_format.pixelSize);
    obj.insert(QStringLiteral("bold"), m_format.bold);
    obj.insert(QStringLiteral("italic"), m_format.italic);
    obj.insert(QStringLiteral("underline"), m_format.underline);
    obj.insert(QStringLiteral("color"), json::fromColor(m_format.color));
    obj.insert(QStringLiteral("align"), static_cast<int>(m_format.alignment));
}

bool TextObject::readProperties(const QJsonObject& obj)
{
    m_text = obj.value(QStringLiteral("text")).toString();
    m_width = std::max(20.0, obj.value(QStringLiteral("width")).toDouble(400));
    m_format.family = obj.value(QStringLiteral("family")).toString();
    m_format.pixelSize = std::clamp(obj.value(QStringLiteral("size")).toInt(40), 6, 800);
    m_format.bold = obj.value(QStringLiteral("bold")).toBool();
    m_format.italic = obj.value(QStringLiteral("italic")).toBool();
    m_format.underline = obj.value(QStringLiteral("underline")).toBool();
    m_format.color = json::toColor(obj.value(QStringLiteral("color")), m_format.color);
    m_format.alignment = static_cast<Qt::Alignment>(obj.value(QStringLiteral("align")).toInt(Qt::AlignLeft));
    m_heightCache = -1;
    return true;
}

} // namespace cb
