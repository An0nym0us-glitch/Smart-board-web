#include "document/PageTemplate.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"
#include "document/ImageStore.h"
#include "math/CoordinateSystem.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace cb {

namespace {
struct KindName
{
    TemplateKind kind;
    const char* name;
};
constexpr KindName kKinds[] = {
    {TemplateKind::Blank, "blank"},       {TemplateKind::Grid, "grid"},
    {TemplateKind::Dots, "dots"},         {TemplateKind::Ruled, "ruled"},
    {TemplateKind::GraphPaper, "graph"},  {TemplateKind::MusicStaff, "music"},
    {TemplateKind::CoordinatePlane, "coordinate"}, {TemplateKind::Image, "image"},
};

QPen cosmeticPen(const QColor& c, qreal width = 1.0)
{
    QPen pen(c, width);
    pen.setCosmetic(true);
    return pen;
}
} // namespace

QString templateKindName(TemplateKind kind)
{
    for (const auto& k : kKinds)
        if (k.kind == kind)
            return QString::fromLatin1(k.name);
    return QStringLiteral("blank");
}

TemplateKind templateKindFromName(const QString& name)
{
    for (const auto& k : kKinds)
        if (name == QLatin1String(k.name))
            return k.kind;
    return TemplateKind::Blank;
}

QJsonObject TemplateSpec::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("kind"), templateKindName(kind));
    o.insert(QStringLiteral("background"), json::fromColor(background));
    o.insert(QStringLiteral("line"), json::fromColor(lineColor));
    o.insert(QStringLiteral("accent"), json::fromColor(accentColor));
    o.insert(QStringLiteral("spacing"), spacing);
    o.insert(QStringLiteral("major"), majorEvery);
    if (!imageKey.isEmpty())
        o.insert(QStringLiteral("image"), imageKey);
    return o;
}

TemplateSpec TemplateSpec::fromJson(const QJsonObject& o)
{
    TemplateSpec t;
    t.id = o.value(QStringLiteral("id")).toString(t.id);
    t.name = o.value(QStringLiteral("name")).toString(t.name);
    t.kind = templateKindFromName(o.value(QStringLiteral("kind")).toString());
    t.background = json::toColor(o.value(QStringLiteral("background")), t.background);
    t.lineColor = json::toColor(o.value(QStringLiteral("line")), t.lineColor);
    t.accentColor = json::toColor(o.value(QStringLiteral("accent")), t.accentColor);
    t.spacing = std::max(4.0, o.value(QStringLiteral("spacing")).toDouble(t.spacing));
    t.majorEvery = std::max(0, o.value(QStringLiteral("major")).toInt(t.majorEvery));
    t.imageKey = o.value(QStringLiteral("image")).toString();
    return t;
}

bool TemplateSpec::operator==(const TemplateSpec& o) const
{
    return id == o.id && name == o.name && kind == o.kind && background == o.background
        && lineColor == o.lineColor && accentColor == o.accentColor && qFuzzyCompare(spacing, o.spacing)
        && majorEvery == o.majorEvery && imageKey == o.imageKey;
}

void TemplateRenderer::paint(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                             const ImageStore* images, const CoordinateSystem& cs, const QRectF& frame)
{
    p.save();
    p.fillRect(visible, spec.background);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QPointF anchor = cs.originPx();
    switch (spec.kind) {
    case TemplateKind::Blank:
        break;
    case TemplateKind::Grid:
        paintGrid(p, spec, visible, zoom, anchor, spec.spacing, spec.majorEvery);
        break;
    case TemplateKind::GraphPaper:
        paintGrid(p, spec, visible, zoom, anchor, spec.spacing, std::max(2, spec.majorEvery));
        break;
    case TemplateKind::Dots:
        paintDots(p, spec, visible, zoom, anchor);
        break;
    case TemplateKind::Ruled:
        paintRuled(p, spec, visible, zoom, frame);
        break;
    case TemplateKind::MusicStaff:
        paintMusic(p, spec, visible, zoom);
        break;
    case TemplateKind::CoordinatePlane:
        paintCoordinatePlane(p, spec, visible, zoom, cs);
        break;
    case TemplateKind::Image:
        if (images && images->contains(spec.imageKey)) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            const QSizeF device(frame.width() * zoom, frame.height() * zoom);
            const QImage img = images->imageForSize(spec.imageKey, device);
            QSizeF s = img.size();
            s.scale(frame.size(), Qt::KeepAspectRatio);
            const QRectF target(frame.center() - QPointF(s.width() / 2, s.height() / 2), s);
            p.drawImage(target, img);
        }
        break;
    }
    p.restore();
}

void TemplateRenderer::paintGrid(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                                 const QPointF& anchor, qreal spacing, int majorEvery)
{
    const qreal screenSpacing = spacing * zoom;
    const bool drawMinor = screenSpacing >= 5.0;
    const bool haveMajor = majorEvery > 1;
    if (!drawMinor && !haveMajor)
        return;
    const qreal step = drawMinor ? spacing : spacing * majorEvery;
    const long long i0 = static_cast<long long>(std::floor((visible.left() - anchor.x()) / step));
    const long long i1 = static_cast<long long>(std::ceil((visible.right() - anchor.x()) / step));
    const long long j0 = static_cast<long long>(std::floor((visible.top() - anchor.y()) / step));
    const long long j1 = static_cast<long long>(std::ceil((visible.bottom() - anchor.y()) / step));
    if ((i1 - i0) > 4000 || (j1 - j0) > 4000)
        return;
    const QPen minorPen = cosmeticPen(spec.lineColor);
    const QPen majorPen = cosmeticPen(spec.accentColor);
    const int ratio = drawMinor ? majorEvery : 1;
    for (long long i = i0; i <= i1; ++i) {
        const qreal x = anchor.x() + i * step;
        const bool major = haveMajor && (!drawMinor || i % ratio == 0);
        p.setPen(major ? majorPen : minorPen);
        p.drawLine(QPointF(x, visible.top()), QPointF(x, visible.bottom()));
    }
    for (long long j = j0; j <= j1; ++j) {
        const qreal y = anchor.y() + j * step;
        const bool major = haveMajor && (!drawMinor || j % ratio == 0);
        p.setPen(major ? majorPen : minorPen);
        p.drawLine(QPointF(visible.left(), y), QPointF(visible.right(), y));
    }
}

void TemplateRenderer::paintDots(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                                 const QPointF& anchor)
{
    qreal step = spec.spacing;
    while (step * zoom < 8.0)
        step *= 2.0;
    const long long i0 = static_cast<long long>(std::floor((visible.left() - anchor.x()) / step));
    const long long i1 = static_cast<long long>(std::ceil((visible.right() - anchor.x()) / step));
    const long long j0 = static_cast<long long>(std::floor((visible.top() - anchor.y()) / step));
    const long long j1 = static_cast<long long>(std::ceil((visible.bottom() - anchor.y()) / step));
    if ((i1 - i0) * (j1 - j0) > 250000)
        return;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(spec.accentColor);
    const qreal r = std::max(1.2 / zoom, 1.6);
    for (long long i = i0; i <= i1; ++i)
        for (long long j = j0; j <= j1; ++j)
            p.drawEllipse(QPointF(anchor.x() + i * step, anchor.y() + j * step), r, r);
}

void TemplateRenderer::paintRuled(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                                  const QRectF& frame)
{
    const qreal step = spec.spacing;
    if (step * zoom >= 4.0) {
        p.setPen(cosmeticPen(spec.lineColor));
        const long long j0 = static_cast<long long>(std::floor((visible.top() - frame.top()) / step));
        const long long j1 = static_cast<long long>(std::ceil((visible.bottom() - frame.top()) / step));
        for (long long j = std::max(2LL, j0); j <= j1; ++j) {
            const qreal y = frame.top() + j * step;
            p.drawLine(QPointF(visible.left(), y), QPointF(visible.right(), y));
        }
    }
    const qreal marginX = frame.left() + step * 3;
    if (marginX >= visible.left() && marginX <= visible.right()) {
        p.setPen(cosmeticPen(spec.accentColor, 1.5));
        p.drawLine(QPointF(marginX, visible.top()), QPointF(marginX, visible.bottom()));
    }
}

void TemplateRenderer::paintMusic(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom)
{
    const qreal gap = spec.spacing;
    if (gap * zoom < 1.2)
        return;
    const qreal period = gap * 11.0; // 5 lines (4 gaps) + generous space between staves
    const qreal topMargin = gap * 4.0;
    p.setPen(cosmeticPen(spec.lineColor, 1.2));
    const long long k0 = static_cast<long long>(std::floor((visible.top() - topMargin) / period)) - 1;
    const long long k1 = static_cast<long long>(std::ceil((visible.bottom() - topMargin) / period));
    for (long long k = k0; k <= k1; ++k) {
        const qreal base = topMargin + k * period;
        for (int line = 0; line < 5; ++line) {
            const qreal y = base + line * gap;
            if (y < visible.top() - 1 || y > visible.bottom() + 1)
                continue;
            p.drawLine(QPointF(visible.left(), y), QPointF(visible.right(), y));
        }
    }
}

void TemplateRenderer::paintCoordinatePlane(QPainter& p, const TemplateSpec& spec, const QRectF& visible,
                                            qreal zoom, const CoordinateSystem& cs)
{
    const qreal unit = cs.pxPerUnit();
    const QPointF origin = cs.originPx();
    // Unit grid, falling back to coarser spacing when zoomed out.
    int every = 1;
    while (unit * every * zoom < 12.0 && every < 100000)
        every *= 2;
    const qreal step = unit * every;
    paintGrid(p, spec, visible, zoom, origin, step, 5);

    // Axes with arrows.
    QPen axisPen = cosmeticPen(spec.accentColor.lighter(160), 2.0);
    QColor axisColor = spec.isDark() ? QColor(235, 240, 235, 210) : QColor(30, 30, 30, 220);
    axisPen.setColor(axisColor);
    p.setPen(axisPen);
    if (origin.y() >= visible.top() && origin.y() <= visible.bottom())
        p.drawLine(QPointF(visible.left(), origin.y()), QPointF(visible.right(), origin.y()));
    if (origin.x() >= visible.left() && origin.x() <= visible.right())
        p.drawLine(QPointF(origin.x(), visible.top()), QPointF(origin.x(), visible.bottom()));

    // Numeric labels: keep at least ~48 screen pixels between labels.
    qreal labelStep = 1.0;
    while (labelStep * unit * zoom < 48.0)
        labelStep = geom::niceNumber(labelStep * 2.0, true);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    // Labels keep a constant on-screen size.
    QFont font;
    font.setPixelSize(14);
    p.save();
    const qreal fontScale = 1.0 / zoom;
    p.setFont(font);
    p.setPen(axisColor);
    auto drawLabel = [&](const QPointF& pagePos, const QString& text, Qt::Alignment align) {
        p.save();
        p.translate(pagePos);
        p.scale(fontScale, fontScale);
        const QRectF box = (align & Qt::AlignHCenter) ? QRectF(-40, 4, 80, 20) : QRectF(-86, -10, 80, 20);
        p.drawText(box, align, text);
        p.restore();
    };
    const QPointF mathTopLeft = cs.toMath(visible.topLeft());
    const QPointF mathBottomRight = cs.toMath(visible.bottomRight());
    const double xMin = std::min(mathTopLeft.x(), mathBottomRight.x());
    const double xMax = std::max(mathTopLeft.x(), mathBottomRight.x());
    const double yMin = std::min(mathTopLeft.y(), mathBottomRight.y());
    const double yMax = std::max(mathTopLeft.y(), mathBottomRight.y());
    if (origin.y() >= visible.top() - 30 && origin.y() <= visible.bottom() + 30) {
        for (double x = std::ceil(xMin / labelStep) * labelStep; x <= xMax; x += labelStep) {
            if (std::abs(x) < labelStep * 0.5)
                continue;
            drawLabel(cs.toPage(QPointF(x, 0)), geom::formatNumber(x, 3), Qt::AlignHCenter | Qt::AlignTop);
        }
    }
    if (origin.x() >= visible.left() - 90 && origin.x() <= visible.right() + 90) {
        for (double y = std::ceil(yMin / labelStep) * labelStep; y <= yMax; y += labelStep) {
            if (std::abs(y) < labelStep * 0.5)
                continue;
            drawLabel(cs.toPage(QPointF(0, y)), geom::formatNumber(y, 3), Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    if (visible.contains(origin))
        drawLabel(origin, QStringLiteral("0"), Qt::AlignRight | Qt::AlignTop);
    p.restore();
}

} // namespace cb
