#include "math/equation/EquationObject.h"

#include "core/JsonUtil.h"

#include <QPainter>

#include <algorithm>

namespace cb {

namespace {
constexpr qreal kPadding = 6.0;

const mathtype::MathTypesetter& typesetter()
{
    // Stateless apart from the chosen font family; safe to share between threads.
    static const mathtype::MathTypesetter instance;
    return instance;
}
} // namespace

EquationObject::EquationObject()
    : DocumentObject(ObjectType::Equation)
{
}

std::unique_ptr<EquationObject> EquationObject::create(const QString& latex, const QPointF& center, qreal pixelSize,
                                                       const QColor& color)
{
    auto e = std::make_unique<EquationObject>();
    e->m_latex = latex;
    e->m_pixelSize = pixelSize;
    e->m_color = color;
    e->setPosition(center);
    return e;
}

void EquationObject::setLatex(const QString& latex)
{
    m_latex = latex;
    m_layout.reset();
    invalidateBounds();
}

void EquationObject::setPixelSize(qreal size)
{
    m_pixelSize = std::clamp(size, 8.0, 600.0);
    m_layout.reset();
    invalidateBounds();
}

const mathtype::Box& EquationObject::layout() const
{
    if (!m_layout)
        m_layout = typesetter().layout(m_latex, m_pixelSize);
    return *m_layout;
}

QRectF EquationObject::localBounds() const
{
    const mathtype::Box& box = layout();
    const qreal w = box.width + 2 * kPadding;
    const qreal h = box.height() + 2 * kPadding;
    return QRectF(-w / 2, -h / 2, w, h);
}

void EquationObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    const mathtype::Box& box = layout();
    const QRectF r = localBounds();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    box.paint(painter, QPointF(r.left() + kPadding, r.top() + kPadding + box.ascent), m_color);
    painter.restore();
}

void EquationObject::paintFormula(QPainter& painter, const QString& latex, const QRectF& target, qreal pixelSize,
                                  const QColor& color)
{
    const mathtype::BoxPtr box = typesetter().layout(latex, pixelSize);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    qreal scale = 1.0;
    if (box->width > target.width() || box->height() > target.height())
        scale = std::min(target.width() / std::max(1.0, box->width), target.height() / std::max(1.0, box->height()));
    painter.translate(target.center());
    painter.scale(scale, scale);
    box->paint(painter, QPointF(-box->width / 2, -box->height() / 2 + box->ascent), color);
    painter.restore();
}

bool EquationObject::setColor(const QColor& color)
{
    m_color = color;
    return true;
}

std::unique_ptr<DocumentObject> EquationObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new EquationObject(*this));
}

void EquationObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    const QRectF old = localBounds();
    if (old.height() > 1.0)
        setPixelSize(m_pixelSize * newSize.height() / old.height());
}

void EquationObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("latex"), m_latex);
    obj.insert(QStringLiteral("size"), m_pixelSize);
    obj.insert(QStringLiteral("color"), json::fromColor(m_color));
}

bool EquationObject::readProperties(const QJsonObject& obj)
{
    m_latex = obj.value(QStringLiteral("latex")).toString();
    m_pixelSize = std::clamp(obj.value(QStringLiteral("size")).toDouble(48.0), 8.0, 600.0);
    m_color = json::toColor(obj.value(QStringLiteral("color")), m_color);
    m_layout.reset();
    return true;
}

} // namespace cb
