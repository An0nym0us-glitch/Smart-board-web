#pragma once

#include "document/DocumentObject.h"
#include "math/equation/MathLayout.h"

#include <memory>

namespace cb {

/// A typeset formula. The source is a LaTeX subset; it can be edited at any time and is
/// re-typeset on demand. The layout is cached and shared between clones.
class EquationObject final : public DocumentObject
{
public:
    EquationObject();
    static std::unique_ptr<EquationObject> create(const QString& latex, const QPointF& center, qreal pixelSize,
                                                  const QColor& color);

    const QString& latex() const { return m_latex; }
    void setLatex(const QString& latex);
    qreal pixelSize() const { return m_pixelSize; }
    void setPixelSize(qreal size);

    QRectF localBounds() const override;
    qreal outlineMargin() const override { return 2.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool keepAspectRatio() const override { return true; }
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_color; }
    bool isEditable() const override { return true; }
    std::unique_ptr<DocumentObject> clone() const override;

    /// Paints a formula preview (used by the equation popover).
    static void paintFormula(QPainter& painter, const QString& latex, const QRectF& target, qreal pixelSize,
                             const QColor& color);

protected:
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    EquationObject(const EquationObject&) = default;
    const mathtype::Box& layout() const;

    QString m_latex;
    qreal m_pixelSize = 48.0;
    QColor m_color = QColor(245, 245, 240);
    mutable std::shared_ptr<const mathtype::Box> m_layout;
};

} // namespace cb
