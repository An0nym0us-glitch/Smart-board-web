#include "math/equation/MathLayout.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace cb::mathtype {

namespace {

enum class Kind { Ord, Op, Bin, Rel, Open, Close, Punct, Inner };

// ----------------------------------------------------------------------------------------- boxes

class KindBox : public Box
{
public:
    Kind kind = Kind::Ord;
    bool limits = false; ///< big operator / lim-like: scripts go above/below
};

class GlyphBox final : public KindBox
{
public:
    GlyphBox(const QString& text, const QFont& font, Kind k, qreal size)
        : m_text(text)
        , m_font(font)
    {
        kind = k;
        const QFontMetricsF fm(font);
        width = fm.horizontalAdvance(text);
        if (font.italic())
            width += size * 0.04;
        const QRectF tight = fm.tightBoundingRect(text);
        ascent = std::max(-tight.top(), fm.xHeight() * 0.9);
        descent = std::max(tight.bottom(), 0.0);
        if (text.trimmed().isEmpty()) {
            ascent = fm.xHeight();
            descent = 0;
        }
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        p.setFont(m_font);
        p.setPen(color);
        p.drawText(pos, m_text);
    }

private:
    QString m_text;
    QFont m_font;
};

class SpaceBox final : public KindBox
{
public:
    explicit SpaceBox(qreal w) { width = w; }
    void paint(QPainter&, const QPointF&, const QColor&) const override {}
};

class RowBox final : public KindBox
{
public:
    struct Item
    {
        BoxPtr box;
        Kind kind;
        qreal x = 0;
    };

    void append(BoxPtr box, Kind k)
    {
        if (!box)
            return;
        m_items.push_back({std::move(box), k, 0});
    }

    bool isEmpty() const { return m_items.empty(); }
    int count() const { return static_cast<int>(m_items.size()); }

    /// Positions children with TeX-like spacing (binary, relation, punctuation, operators).
    void finish(qreal size)
    {
        const qreal em = size;
        qreal x = 0;
        ascent = 0;
        descent = 0;
        Kind prev = Kind::Open;
        bool first = true;
        for (Item& it : m_items) {
            Kind k = it.kind;
            if (k == Kind::Bin && (first || prev == Kind::Bin || prev == Kind::Rel || prev == Kind::Open
                                   || prev == Kind::Punct || prev == Kind::Op))
                k = Kind::Ord; // unary minus / plus
            it.kind = k;
            if (!first) {
                qreal space = 0;
                if (k == Kind::Bin || prev == Kind::Bin)
                    space = 0.22 * em;
                else if (k == Kind::Rel || prev == Kind::Rel)
                    space = 0.28 * em;
                else if (prev == Kind::Punct)
                    space = 0.17 * em;
                else if (prev == Kind::Op && (k == Kind::Ord || k == Kind::Inner))
                    space = 0.14 * em;
                else if (k == Kind::Op && (prev == Kind::Ord || prev == Kind::Close))
                    space = 0.14 * em;
                x += space;
            }
            it.x = x;
            x += it.box->width;
            ascent = std::max(ascent, it.box->ascent);
            descent = std::max(descent, it.box->descent);
            prev = k;
            first = false;
        }
        width = x;
        if (m_items.empty()) {
            ascent = size * 0.5;
            descent = 0;
            width = size * 0.3;
        }
        // A row with a single item inherits its class (for scripts on groups).
        if (m_items.size() == 1) {
            kind = m_items.front().kind;
            if (auto* kb = dynamic_cast<KindBox*>(m_items.front().box.get()))
                limits = kb->limits;
        } else {
            kind = Kind::Ord;
        }
    }

    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        for (const Item& it : m_items)
            it.box->paint(p, pos + QPointF(it.x, 0), color);
    }

private:
    std::vector<Item> m_items;
};

class FracBox final : public KindBox
{
public:
    FracBox(BoxPtr num, BoxPtr den, qreal size, bool bar)
        : m_num(std::move(num))
        , m_den(std::move(den))
        , m_bar(bar)
    {
        kind = Kind::Inner;
        m_axis = size * 0.27;
        m_rule = std::max(1.0, size * 0.055);
        const qreal gap = size * 0.14;
        m_pad = size * 0.12;
        width = std::max(m_num->width, m_den->width) + 2 * m_pad;
        m_numY = -(m_axis + m_rule / 2 + gap + m_num->descent);
        m_denY = -m_axis + m_rule / 2 + gap + m_den->ascent;
        ascent = -m_numY + m_num->ascent;
        descent = std::max(0.0, m_denY + m_den->descent);
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        m_num->paint(p, pos + QPointF((width - m_num->width) / 2, m_numY), color);
        m_den->paint(p, pos + QPointF((width - m_den->width) / 2, m_denY), color);
        if (m_bar) {
            p.setPen(QPen(color, m_rule, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(pos + QPointF(m_pad * 0.4, -m_axis), pos + QPointF(width - m_pad * 0.4, -m_axis));
        }
    }

private:
    BoxPtr m_num, m_den;
    bool m_bar;
    qreal m_axis, m_rule, m_pad, m_numY, m_denY;
};

class RadicalBox final : public KindBox
{
public:
    RadicalBox(BoxPtr body, BoxPtr index, qreal size)
        : m_body(std::move(body))
        , m_index(std::move(index))
    {
        m_rule = std::max(1.0, size * 0.055);
        const qreal gap = size * 0.12;
        m_top = -(m_body->ascent + gap + m_rule / 2);
        m_bottom = m_body->descent + size * 0.08;
        const qreal h = m_bottom - m_top;
        m_signW = std::min(size * 0.9, size * 0.42 + h * 0.12);
        m_offset = 0;
        if (m_index)
            m_offset = std::max(0.0, m_index->width - m_signW * 0.45);
        m_bodyX = m_offset + m_signW + size * 0.06;
        width = m_bodyX + m_body->width + size * 0.12;
        ascent = -m_top + m_rule / 2;
        descent = m_bottom;
        m_size = size;
        if (m_index) {
            const qreal indexBaseline = m_bottom - h * 0.52;
            ascent = std::max(ascent, -indexBaseline + m_index->ascent);
            m_indexY = indexBaseline;
        }
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        const qreal h = m_bottom - m_top;
        const qreal x0 = pos.x() + m_offset;
        const qreal yMid = pos.y() + m_bottom - h * 0.42;
        QPainterPath path;
        path.moveTo(x0, yMid + m_size * 0.02);
        path.lineTo(x0 + m_signW * 0.2, yMid - m_size * 0.07);
        path.lineTo(x0 + m_signW * 0.5, pos.y() + m_bottom);
        path.lineTo(x0 + m_signW, pos.y() + m_top);
        path.lineTo(pos.x() + width - m_size * 0.04, pos.y() + m_top);
        p.setPen(QPen(color, m_rule, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        m_body->paint(p, pos + QPointF(m_bodyX, 0), color);
        if (m_index)
            m_index->paint(p, QPointF(pos.x() + std::max(0.0, m_offset + m_signW * 0.45 - m_index->width), pos.y() + m_indexY), color);
    }

private:
    BoxPtr m_body, m_index;
    qreal m_rule = 1, m_top = 0, m_bottom = 0, m_signW = 0, m_offset = 0, m_bodyX = 0, m_size = 0, m_indexY = 0;
};

class ScriptBox final : public KindBox
{
public:
    ScriptBox(BoxPtr base, BoxPtr sup, BoxPtr sub, qreal size, Kind baseKind)
        : m_base(std::move(base))
        , m_sup(std::move(sup))
        , m_sub(std::move(sub))
    {
        kind = baseKind;
        qreal u = 0, d = 0;
        if (m_sup)
            u = std::max(size * 0.42, m_base->ascent - size * 0.32);
        if (m_sub)
            d = std::max(size * 0.2, m_base->descent - size * 0.05);
        if (m_sup && m_sub) {
            const qreal gap = (u - m_sup->descent) - (m_sub->ascent - d);
            if (gap < size * 0.12)
                d += size * 0.12 - gap;
        }
        m_supY = -u;
        m_subY = d;
        const qreal scriptX = m_base->width + size * 0.03;
        m_scriptX = scriptX;
        width = scriptX + std::max(m_sup ? m_sup->width : 0.0, m_sub ? m_sub->width : 0.0) + size * 0.04;
        ascent = std::max(m_base->ascent, m_sup ? u + m_sup->ascent : 0.0);
        descent = std::max(m_base->descent, m_sub ? d + m_sub->descent : 0.0);
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        m_base->paint(p, pos, color);
        if (m_sup)
            m_sup->paint(p, pos + QPointF(m_scriptX, m_supY), color);
        if (m_sub)
            m_sub->paint(p, pos + QPointF(m_scriptX, m_subY), color);
    }

private:
    BoxPtr m_base, m_sup, m_sub;
    qreal m_supY = 0, m_subY = 0, m_scriptX = 0;
};

class LimitsBox final : public KindBox
{
public:
    LimitsBox(BoxPtr nucleus, BoxPtr over, BoxPtr under, qreal size)
        : m_nucleus(std::move(nucleus))
        , m_over(std::move(over))
        , m_under(std::move(under))
    {
        kind = Kind::Op;
        const qreal gap = size * 0.12;
        width = std::max({m_nucleus->width, m_over ? m_over->width : 0.0, m_under ? m_under->width : 0.0});
        m_overY = -(m_nucleus->ascent + gap + (m_over ? m_over->descent : 0));
        m_underY = m_nucleus->descent + gap + (m_under ? m_under->ascent : 0);
        ascent = m_over ? -m_overY + m_over->ascent : m_nucleus->ascent;
        descent = m_under ? m_underY + m_under->descent : m_nucleus->descent;
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        m_nucleus->paint(p, pos + QPointF((width - m_nucleus->width) / 2, 0), color);
        if (m_over)
            m_over->paint(p, pos + QPointF((width - m_over->width) / 2, m_overY), color);
        if (m_under)
            m_under->paint(p, pos + QPointF((width - m_under->width) / 2, m_underY), color);
    }

private:
    BoxPtr m_nucleus, m_over, m_under;
    qreal m_overY = 0, m_underY = 0;
};

/// A glyph drawn larger and centred on the math axis (big operators).
class BigGlyphBox final : public KindBox
{
public:
    BigGlyphBox(const QString& text, const QFont& font, qreal size)
        : m_text(text)
        , m_font(font)
    {
        kind = Kind::Op;
        const QFontMetricsF fm(font);
        const QRectF tight = fm.tightBoundingRect(text);
        width = fm.horizontalAdvance(text) + size * 0.08;
        const qreal axis = size * 0.27;
        const qreal h = tight.height();
        // Shift the glyph so its tight box is centred on the axis.
        m_shift = -axis - (tight.top() + h / 2);
        ascent = -(tight.top() + m_shift);
        descent = tight.bottom() + m_shift;
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        p.setFont(m_font);
        p.setPen(color);
        p.drawText(pos + QPointF(0, m_shift), m_text);
    }

private:
    QString m_text;
    QFont m_font;
    qreal m_shift = 0;
};

enum class AccentKind { Vec, Hat, Bar, Dot, DDot, Tilde, Underline };

class AccentBox final : public KindBox
{
public:
    AccentBox(BoxPtr body, AccentKind accent, qreal size)
        : m_body(std::move(body))
        , m_accent(accent)
        , m_size(size)
    {
        width = std::max(m_body->width, accent == AccentKind::Vec ? size * 0.55 : 0.0);
        m_gap = size * 0.1;
        m_h = accent == AccentKind::Underline ? 0 : size * 0.22;
        ascent = m_body->ascent + m_gap + m_h;
        descent = m_body->descent + (accent == AccentKind::Underline ? size * 0.18 : 0);
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        m_body->paint(p, pos + QPointF((width - m_body->width) / 2, 0), color);
        const qreal rule = std::max(1.0, m_size * 0.05);
        const qreal y = pos.y() - m_body->ascent - m_gap - m_h * 0.5;
        const qreal x0 = pos.x() + m_size * 0.04;
        const qreal x1 = pos.x() + width - m_size * 0.02;
        const qreal cx = (x0 + x1) / 2;
        p.setPen(QPen(color, rule, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        switch (m_accent) {
        case AccentKind::Vec: {
            p.drawLine(QPointF(x0, y), QPointF(x1, y));
            const qreal head = m_size * 0.14;
            p.drawLine(QPointF(x1, y), QPointF(x1 - head, y - head * 0.7));
            p.drawLine(QPointF(x1, y), QPointF(x1 - head, y + head * 0.7));
            break;
        }
        case AccentKind::Hat: {
            const qreal w = std::min(width * 0.5, m_size * 0.35);
            p.drawLine(QPointF(cx - w, y + m_h * 0.4), QPointF(cx, y - m_h * 0.4));
            p.drawLine(QPointF(cx, y - m_h * 0.4), QPointF(cx + w, y + m_h * 0.4));
            break;
        }
        case AccentKind::Bar:
            p.drawLine(QPointF(x0, y), QPointF(x1, y));
            break;
        case AccentKind::Dot:
            p.setBrush(color);
            p.drawEllipse(QPointF(cx, y), rule * 1.1, rule * 1.1);
            break;
        case AccentKind::DDot:
            p.setBrush(color);
            p.drawEllipse(QPointF(cx - m_size * 0.1, y), rule * 1.1, rule * 1.1);
            p.drawEllipse(QPointF(cx + m_size * 0.1, y), rule * 1.1, rule * 1.1);
            break;
        case AccentKind::Tilde: {
            const qreal w = std::min(width * 0.5, m_size * 0.3);
            QPainterPath path(QPointF(cx - w, y + m_h * 0.2));
            path.cubicTo(QPointF(cx - w * 0.4, y - m_h * 0.6), QPointF(cx + w * 0.4, y + m_h * 0.6),
                         QPointF(cx + w, y - m_h * 0.2));
            p.drawPath(path);
            break;
        }
        case AccentKind::Underline: {
            const qreal uy = pos.y() + m_body->descent + m_size * 0.1;
            p.drawLine(QPointF(x0, uy), QPointF(x1, uy));
            break;
        }
        }
    }

private:
    BoxPtr m_body;
    AccentKind m_accent;
    qreal m_size;
    qreal m_gap = 0;
    qreal m_h = 0;
};

/// Draws a stretchy delimiter of a given total height centred on the axis.
void paintDelimiter(QPainter& p, QChar d, const QRectF& r, const QColor& color, qreal rule)
{
    p.setPen(QPen(color, rule, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const qreal l = r.left() + r.width() * 0.2;
    const qreal rr = r.right() - r.width() * 0.2;
    const qreal cy = r.center().y();
    switch (d.unicode()) {
    case '(': {
        QPainterPath path(QPointF(rr, r.top()));
        path.quadTo(QPointF(l - r.width() * 0.25, cy), QPointF(rr, r.bottom()));
        p.drawPath(path);
        break;
    }
    case ')': {
        QPainterPath path(QPointF(l, r.top()));
        path.quadTo(QPointF(rr + r.width() * 0.25, cy), QPointF(l, r.bottom()));
        p.drawPath(path);
        break;
    }
    case '[':
        p.drawPolyline(QPolygonF({QPointF(rr, r.top()), QPointF(l, r.top()), QPointF(l, r.bottom()), QPointF(rr, r.bottom())}));
        break;
    case ']':
        p.drawPolyline(QPolygonF({QPointF(l, r.top()), QPointF(rr, r.top()), QPointF(rr, r.bottom()), QPointF(l, r.bottom())}));
        break;
    case '{':
    case '}': {
        const bool left = d == QLatin1Char('{');
        const qreal outer = left ? rr : l;
        const qreal inner = left ? l : rr;
        const qreal mid = (outer + inner) / 2;
        QPainterPath path(QPointF(outer, r.top()));
        path.quadTo(QPointF(mid, r.top()), QPointF(mid, r.top() + r.height() * 0.12));
        path.lineTo(QPointF(mid, cy - r.height() * 0.1));
        path.quadTo(QPointF(mid, cy), QPointF(inner, cy));
        path.quadTo(QPointF(mid, cy), QPointF(mid, cy + r.height() * 0.1));
        path.lineTo(QPointF(mid, r.bottom() - r.height() * 0.12));
        path.quadTo(QPointF(mid, r.bottom()), QPointF(outer, r.bottom()));
        p.drawPath(path);
        break;
    }
    case '|':
        p.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.bottom()));
        break;
    case 0x2016: // ‖
        p.drawLine(QPointF(r.center().x() - r.width() * 0.15, r.top()), QPointF(r.center().x() - r.width() * 0.15, r.bottom()));
        p.drawLine(QPointF(r.center().x() + r.width() * 0.15, r.top()), QPointF(r.center().x() + r.width() * 0.15, r.bottom()));
        break;
    case 0x27E8: // ⟨
        p.drawPolyline(QPolygonF({QPointF(rr, r.top()), QPointF(l, cy), QPointF(rr, r.bottom())}));
        break;
    case 0x27E9: // ⟩
        p.drawPolyline(QPolygonF({QPointF(l, r.top()), QPointF(rr, cy), QPointF(l, r.bottom())}));
        break;
    default:
        break;
    }
}

class DelimBox final : public KindBox
{
public:
    DelimBox(QChar left, BoxPtr body, QChar right, qreal size)
        : m_left(left)
        , m_right(right)
        , m_body(std::move(body))
        , m_size(size)
    {
        kind = Kind::Inner;
        const qreal axis = size * 0.27;
        const qreal half = std::max(m_body->ascent - axis, m_body->descent + axis) + size * 0.12;
        m_top = -axis - half;
        m_bottom = -axis + half;
        m_delimW = std::min(size * 0.7, size * 0.32 + (m_bottom - m_top) * 0.04);
        const qreal lw = m_left.isNull() || m_left == QLatin1Char('.') ? 0 : m_delimW;
        const qreal rw = m_right.isNull() || m_right == QLatin1Char('.') ? 0 : m_delimW;
        m_bodyX = lw + size * 0.04;
        width = m_bodyX + m_body->width + size * 0.04 + rw;
        ascent = -m_top;
        descent = std::max(0.0, m_bottom);
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        const qreal rule = std::max(1.0, m_size * 0.06);
        if (!m_left.isNull() && m_left != QLatin1Char('.'))
            paintDelimiter(p, m_left, QRectF(pos.x(), pos.y() + m_top, m_delimW, m_bottom - m_top), color, rule);
        m_body->paint(p, pos + QPointF(m_bodyX, 0), color);
        if (!m_right.isNull() && m_right != QLatin1Char('.'))
            paintDelimiter(p, m_right, QRectF(pos.x() + width - m_delimW, pos.y() + m_top, m_delimW, m_bottom - m_top),
                           color, rule);
    }

private:
    QChar m_left, m_right;
    BoxPtr m_body;
    qreal m_size;
    qreal m_top = 0, m_bottom = 0, m_delimW = 0, m_bodyX = 0;
};

class MatrixBox final : public KindBox
{
public:
    MatrixBox(std::vector<std::vector<BoxPtr>> cells, qreal size, bool leftAlign)
        : m_cells(std::move(cells))
        , m_leftAlign(leftAlign)
    {
        kind = Kind::Inner;
        size_t cols = 0;
        for (const auto& row : m_cells)
            cols = std::max(cols, row.size());
        m_colWidths.assign(cols, 0.0);
        for (const auto& row : m_cells)
            for (size_t c = 0; c < row.size(); ++c)
                m_colWidths[c] = std::max(m_colWidths[c], row[c]->width);
        const qreal colGap = size * (leftAlign ? 1.0 : 0.9);
        const qreal rowGap = size * 0.35;
        qreal y = 0;
        for (const auto& row : m_cells) {
            qreal a = size * 0.5, d = size * 0.15;
            for (const auto& cell : row) {
                a = std::max(a, cell->ascent);
                d = std::max(d, cell->descent);
            }
            y += a;
            m_rowBaselines.push_back(y);
            y += d + rowGap;
        }
        const qreal total = std::max(0.0, y - rowGap);
        const qreal axis = size * 0.27;
        m_offsetY = -axis - total / 2;
        width = 0;
        for (size_t c = 0; c < cols; ++c)
            width += m_colWidths[c] + (c + 1 < cols ? colGap : 0);
        m_colGap = colGap;
        ascent = -m_offsetY;
        descent = std::max(0.0, total + m_offsetY);
    }
    void paint(QPainter& p, const QPointF& pos, const QColor& color) const override
    {
        for (size_t r = 0; r < m_cells.size(); ++r) {
            qreal x = pos.x();
            for (size_t c = 0; c < m_colWidths.size(); ++c) {
                if (c < m_cells[r].size()) {
                    const Box& cell = *m_cells[r][c];
                    const qreal cx = m_leftAlign ? x : x + (m_colWidths[c] - cell.width) / 2;
                    cell.paint(p, QPointF(cx, pos.y() + m_offsetY + m_rowBaselines[r]), color);
                }
                x += m_colWidths[c] + m_colGap;
            }
        }
    }

private:
    std::vector<std::vector<BoxPtr>> m_cells;
    bool m_leftAlign;
    std::vector<qreal> m_colWidths;
    std::vector<qreal> m_rowBaselines;
    qreal m_offsetY = 0;
    qreal m_colGap = 0;
};

// ---------------------------------------------------------------------------------- symbol table

struct Symbol
{
    const char* name;
    ushort code;
    Kind kind;
};

const Symbol kSymbols[] = {
    // Greek lowercase
    {"alpha", 0x03B1, Kind::Ord}, {"beta", 0x03B2, Kind::Ord}, {"gamma", 0x03B3, Kind::Ord},
    {"delta", 0x03B4, Kind::Ord}, {"epsilon", 0x03B5, Kind::Ord}, {"varepsilon", 0x03B5, Kind::Ord},
    {"zeta", 0x03B6, Kind::Ord}, {"eta", 0x03B7, Kind::Ord}, {"theta", 0x03B8, Kind::Ord},
    {"vartheta", 0x03D1, Kind::Ord}, {"iota", 0x03B9, Kind::Ord}, {"kappa", 0x03BA, Kind::Ord},
    {"lambda", 0x03BB, Kind::Ord}, {"mu", 0x03BC, Kind::Ord}, {"nu", 0x03BD, Kind::Ord},
    {"xi", 0x03BE, Kind::Ord}, {"pi", 0x03C0, Kind::Ord}, {"rho", 0x03C1, Kind::Ord},
    {"sigma", 0x03C3, Kind::Ord}, {"tau", 0x03C4, Kind::Ord}, {"upsilon", 0x03C5, Kind::Ord},
    {"phi", 0x03C6, Kind::Ord}, {"varphi", 0x03C6, Kind::Ord}, {"chi", 0x03C7, Kind::Ord},
    {"psi", 0x03C8, Kind::Ord}, {"omega", 0x03C9, Kind::Ord},
    // Greek uppercase
    {"Gamma", 0x0393, Kind::Ord}, {"Delta", 0x0394, Kind::Ord}, {"Theta", 0x0398, Kind::Ord},
    {"Lambda", 0x039B, Kind::Ord}, {"Xi", 0x039E, Kind::Ord}, {"Pi", 0x03A0, Kind::Ord},
    {"Sigma", 0x03A3, Kind::Ord}, {"Phi", 0x03A6, Kind::Ord}, {"Psi", 0x03A8, Kind::Ord},
    {"Omega", 0x03A9, Kind::Ord},
    // Binary operators
    {"pm", 0x00B1, Kind::Bin}, {"mp", 0x2213, Kind::Bin}, {"times", 0x00D7, Kind::Bin},
    {"div", 0x00F7, Kind::Bin}, {"cdot", 0x22C5, Kind::Bin}, {"ast", 0x2217, Kind::Bin},
    {"circ", 0x2218, Kind::Bin}, {"cup", 0x222A, Kind::Bin}, {"cap", 0x2229, Kind::Bin},
    {"setminus", 0x2216, Kind::Bin}, {"oplus", 0x2295, Kind::Bin}, {"otimes", 0x2297, Kind::Bin},
    {"wedge", 0x2227, Kind::Bin}, {"vee", 0x2228, Kind::Bin},
    // Relations and arrows
    {"leq", 0x2264, Kind::Rel}, {"le", 0x2264, Kind::Rel}, {"geq", 0x2265, Kind::Rel},
    {"ge", 0x2265, Kind::Rel}, {"neq", 0x2260, Kind::Rel}, {"ne", 0x2260, Kind::Rel},
    {"approx", 0x2248, Kind::Rel}, {"equiv", 0x2261, Kind::Rel}, {"sim", 0x223C, Kind::Rel},
    {"simeq", 0x2243, Kind::Rel}, {"cong", 0x2245, Kind::Rel}, {"propto", 0x221D, Kind::Rel},
    {"ll", 0x226A, Kind::Rel}, {"gg", 0x226B, Kind::Rel}, {"in", 0x2208, Kind::Rel},
    {"notin", 0x2209, Kind::Rel}, {"ni", 0x220B, Kind::Rel}, {"subset", 0x2282, Kind::Rel},
    {"supset", 0x2283, Kind::Rel}, {"subseteq", 0x2286, Kind::Rel}, {"supseteq", 0x2287, Kind::Rel},
    {"perp", 0x22A5, Kind::Rel}, {"parallel", 0x2225, Kind::Rel}, {"mid", 0x2223, Kind::Rel},
    {"to", 0x2192, Kind::Rel}, {"rightarrow", 0x2192, Kind::Rel}, {"leftarrow", 0x2190, Kind::Rel},
    {"gets", 0x2190, Kind::Rel}, {"leftrightarrow", 0x2194, Kind::Rel}, {"Rightarrow", 0x21D2, Kind::Rel},
    {"implies", 0x21D2, Kind::Rel}, {"Leftarrow", 0x21D0, Kind::Rel}, {"Leftrightarrow", 0x21D4, Kind::Rel},
    {"iff", 0x21D4, Kind::Rel}, {"mapsto", 0x21A6, Kind::Rel}, {"uparrow", 0x2191, Kind::Rel},
    {"downarrow", 0x2193, Kind::Rel},
    // Ordinary symbols
    {"infty", 0x221E, Kind::Ord}, {"partial", 0x2202, Kind::Ord}, {"nabla", 0x2207, Kind::Ord},
    {"forall", 0x2200, Kind::Ord}, {"exists", 0x2203, Kind::Ord}, {"emptyset", 0x2205, Kind::Ord},
    {"varnothing", 0x2205, Kind::Ord}, {"angle", 0x2220, Kind::Ord}, {"triangle", 0x25B3, Kind::Ord},
    {"degree", 0x00B0, Kind::Ord}, {"prime", 0x2032, Kind::Ord}, {"ldots", 0x2026, Kind::Inner},
    {"cdots", 0x22EF, Kind::Inner}, {"dots", 0x2026, Kind::Inner}, {"vdots", 0x22EE, Kind::Ord},
    {"ddots", 0x22F1, Kind::Ord}, {"hbar", 0x210F, Kind::Ord}, {"ell", 0x2113, Kind::Ord},
    {"Re", 0x211C, Kind::Ord}, {"Im", 0x2111, Kind::Ord}, {"aleph", 0x2135, Kind::Ord},
    {"neg", 0x00AC, Kind::Ord}, {"therefore", 0x2234, Kind::Rel}, {"because", 0x2235, Kind::Rel},
    {"langle", 0x27E8, Kind::Open}, {"rangle", 0x27E9, Kind::Close}, {"lvert", 0x007C, Kind::Open},
    {"rvert", 0x007C, Kind::Close}, {"|", 0x2016, Kind::Ord}, {"{", 0x007B, Kind::Open},
    {"}", 0x007D, Kind::Close}, {"%", 0x0025, Kind::Ord}, {"#", 0x0023, Kind::Ord}, {"$", 0x0024, Kind::Ord},
    {"&", 0x0026, Kind::Ord}, {"_", 0x005F, Kind::Ord},
};

const Symbol* findSymbol(const QString& name)
{
    for (const auto& s : kSymbols)
        if (name == QLatin1String(s.name))
            return &s;
    return nullptr;
}

struct BigOp
{
    const char* name;
    ushort code;
    bool limits;
};
const BigOp kBigOps[] = {
    {"sum", 0x2211, true},    {"prod", 0x220F, true},   {"coprod", 0x2210, true}, {"bigcup", 0x22C3, true},
    {"bigcap", 0x22C2, true}, {"int", 0x222B, false},   {"iint", 0x222C, false},  {"iiint", 0x222D, false},
    {"oint", 0x222E, false},
};

const char* const kFunctions[] = {"sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan", "sinh",
                                  "cosh", "tanh", "log", "ln", "lg", "exp", "deg", "dim", "arg", "ker", "hom"};
const char* const kLimitFunctions[] = {"lim", "max", "min", "sup", "inf", "det", "gcd", "limsup", "liminf"};

Kind classifyChar(QChar c)
{
    switch (c.unicode()) {
    case '+': case '-': case 0x2212: case '*': case 0x00B1: case 0x00D7: case 0x00F7: case 0x22C5: case 0x00B7:
    case 0x222A: case 0x2229: case 0x2218:
        return Kind::Bin;
    case '=': case '<': case '>': case ':': case 0x2264: case 0x2265: case 0x2260: case 0x2248: case 0x2261:
    case 0x2192: case 0x2190: case 0x21D2: case 0x21D4: case 0x2208: case 0x2282: case 0x2286: case 0x223C:
        return Kind::Rel;
    case '(': case '[':
        return Kind::Open;
    case ')': case ']': case '!':
        return Kind::Close;
    case ',': case ';':
        return Kind::Punct;
    default:
        return Kind::Ord;
    }
}

// ---------------------------------------------------------------------------------------- parser

class Parser
{
public:
    Parser(const QString& src, const QString& family)
        : m_s(src)
        , m_family(family)
    {
    }

    BoxPtr parseAll(qreal size)
    {
        auto row = parseRow(size);
        // Unbalanced closing braces etc.: continue after them so nothing is lost.
        while (m_i < m_s.size()) {
            if (lookingAt(QStringLiteral("\\right"))) {
                m_i += 6;
                readDelimiter();
            } else if (lookingAt(QStringLiteral("\\end"))) {
                m_i += 4;
                readRawGroup();
            } else if (lookingAt(QStringLiteral("\\\\"))) {
                m_i += 2;
            } else {
                ++m_i;
            }
            auto more = parseRow(size);
            auto combined = std::make_unique<RowBox>();
            combined->append(std::move(row), Kind::Ord);
            combined->append(std::move(more), Kind::Ord);
            combined->finish(size);
            row = std::move(combined);
        }
        return row;
    }

private:
    QFont font(qreal size, bool italic, bool bold = false) const
    {
        QFont f(m_family);
        f.setPixelSize(std::max(4, static_cast<int>(std::lround(size))));
        f.setItalic(italic);
        f.setBold(bold);
        f.setHintingPreference(QFont::PreferNoHinting);
        return f;
    }

    static qreal scriptSize(qreal size) { return std::max(size * 0.7, 6.0); }

    void skipSpaces()
    {
        while (m_i < m_s.size() && m_s[m_i].isSpace())
            ++m_i;
    }

    bool atEnd() const { return m_i >= m_s.size(); }

    bool lookingAt(const QString& text) const { return m_s.midRef(m_i, text.size()) == text; }

    /// Reads a command name after a backslash.
    QString readCommand()
    {
        ++m_i; // backslash
        if (atEnd())
            return QString();
        if (!m_s[m_i].isLetter())
            return QString(m_s[m_i++]);
        const int start = m_i;
        while (m_i < m_s.size() && m_s[m_i].isLetter())
            ++m_i;
        return m_s.mid(start, m_i - start);
    }

    /// Reads a raw {text} argument (for \text, \begin ...).
    QString readRawGroup()
    {
        skipSpaces();
        if (atEnd() || m_s[m_i] != QLatin1Char('{'))
            return QString();
        ++m_i;
        int depth = 1;
        const int start = m_i;
        while (m_i < m_s.size()) {
            if (m_s[m_i] == QLatin1Char('{'))
                ++depth;
            else if (m_s[m_i] == QLatin1Char('}') && --depth == 0)
                break;
            ++m_i;
        }
        const QString text = m_s.mid(start, m_i - start);
        if (m_i < m_s.size())
            ++m_i;
        return text;
    }

    /// True when the row must stop (group end, cell/row separator, \right, \end).
    bool atRowTerminator() const
    {
        if (atEnd())
            return true;
        const QChar c = m_s[m_i];
        if (c == QLatin1Char('}') || c == QLatin1Char('&'))
            return true;
        return lookingAt(QStringLiteral("\\\\")) || lookingAt(QStringLiteral("\\right"))
            || lookingAt(QStringLiteral("\\end"));
    }

    std::unique_ptr<RowBox> parseRow(qreal size)
    {
        auto row = std::make_unique<RowBox>();
        while (true) {
            skipSpaces();
            if (atRowTerminator())
                break;
            Kind kind = Kind::Ord;
            BoxPtr atom = parseAtom(size, &kind);
            if (!atom)
                continue;
            atom = parseScripts(std::move(atom), size, &kind);
            row->append(std::move(atom), kind);
        }
        row->finish(size);
        return row;
    }

    BoxPtr parseScripts(BoxPtr base, qreal size, Kind* kind)
    {
        BoxPtr sup, sub;
        while (true) {
            skipSpaces();
            if (atEnd())
                break;
            const QChar c = m_s[m_i];
            if (c == QLatin1Char('^') && !sup) {
                ++m_i;
                sup = parseArgument(scriptSize(size));
            } else if (c == QLatin1Char('_') && !sub) {
                ++m_i;
                sub = parseArgument(scriptSize(size));
            } else if (c == QLatin1Char('\'') && !sup) {
                ++m_i;
                sup = std::make_unique<GlyphBox>(QString(QChar(0x2032)), font(scriptSize(size), false), Kind::Ord, size);
            } else {
                break;
            }
        }
        if (!sup && !sub)
            return base;
        const auto* kb = dynamic_cast<const KindBox*>(base.get());
        if (kb && kb->limits)
            return std::make_unique<LimitsBox>(std::move(base), std::move(sup), std::move(sub), size);
        return std::make_unique<ScriptBox>(std::move(base), std::move(sup), std::move(sub), size, *kind);
    }

    /// A single token or a {group}.
    BoxPtr parseArgument(qreal size)
    {
        skipSpaces();
        if (atEnd())
            return std::make_unique<RowBox>();
        if (m_s[m_i] == QLatin1Char('{')) {
            ++m_i;
            auto row = parseRow(size);
            if (!atEnd() && m_s[m_i] == QLatin1Char('}'))
                ++m_i;
            return row;
        }
        Kind kind = Kind::Ord;
        if (m_s[m_i].isDigit())
            return glyph(QString(m_s[m_i++]), size, false, Kind::Ord);
        return parseAtom(size, &kind);
    }

    BoxPtr glyph(const QString& text, qreal size, bool italic, Kind kind, bool bold = false)
    {
        auto g = std::make_unique<GlyphBox>(text, font(size, italic, bold), kind, size);
        return g;
    }

    BoxPtr parseAtom(qreal size, Kind* kind)
    {
        const QChar c = m_s[m_i];
        if (c == QLatin1Char('{')) {
            ++m_i;
            auto row = parseRow(size);
            if (!atEnd() && m_s[m_i] == QLatin1Char('}'))
                ++m_i;
            *kind = row->count() == 1 ? row->kind : Kind::Ord;
            return row;
        }
        if (c == QLatin1Char('\\'))
            return parseCommand(size, kind);
        if (c.isDigit() || (c == QLatin1Char('.') && m_i + 1 < m_s.size() && m_s[m_i + 1].isDigit())) {
            const int start = m_i;
            while (m_i < m_s.size() && (m_s[m_i].isDigit() || m_s[m_i] == QLatin1Char('.')))
                ++m_i;
            *kind = Kind::Ord;
            return glyph(m_s.mid(start, m_i - start), size, false, Kind::Ord);
        }
        ++m_i;
        if (c == QLatin1Char('^') || c == QLatin1Char('_')) {
            // Script without a base: attach to an empty box.
            --m_i;
            *kind = Kind::Ord;
            return std::make_unique<SpaceBox>(0);
        }
        if (c.isLetter()) {
            *kind = Kind::Ord;
            const bool upperGreek = c.unicode() >= 0x0391 && c.unicode() <= 0x03A9;
            return glyph(QString(c), size, !upperGreek, Kind::Ord);
        }
        QString text(c);
        if (c == QLatin1Char('-'))
            text = QString(QChar(0x2212));
        else if (c == QLatin1Char('*'))
            text = QString(QChar(0x22C5));
        else if (c == QLatin1Char('~')) {
            *kind = Kind::Ord;
            return std::make_unique<SpaceBox>(size * 0.33);
        }
        *kind = classifyChar(c);
        return glyph(text, size, false, *kind);
    }

    QChar readDelimiter()
    {
        skipSpaces();
        if (atEnd())
            return QChar();
        if (m_s[m_i] == QLatin1Char('\\')) {
            const QString cmd = readCommand();
            if (cmd == QLatin1String("{"))
                return QLatin1Char('{');
            if (cmd == QLatin1String("}"))
                return QLatin1Char('}');
            if (cmd == QLatin1String("langle"))
                return QChar(0x27E8);
            if (cmd == QLatin1String("rangle"))
                return QChar(0x27E9);
            if (cmd == QLatin1String("|") || cmd == QLatin1String("Vert"))
                return QChar(0x2016);
            if (cmd == QLatin1String("lvert") || cmd == QLatin1String("rvert") || cmd == QLatin1String("vert"))
                return QLatin1Char('|');
            return QLatin1Char('.');
        }
        return m_s[m_i++];
    }

    BoxPtr parseMatrix(const QString& env, qreal size, Kind* kind)
    {
        std::vector<std::vector<BoxPtr>> rows(1);
        const qreal cellSize = env == QLatin1String("smallmatrix") ? size * 0.75 : size;
        while (!atEnd()) {
            auto cell = parseRow(cellSize);
            rows.back().push_back(std::move(cell));
            if (atEnd())
                break;
            if (m_s[m_i] == QLatin1Char('&')) {
                ++m_i;
                continue;
            }
            if (lookingAt(QStringLiteral("\\\\"))) {
                m_i += 2;
                rows.emplace_back();
                continue;
            }
            if (lookingAt(QStringLiteral("\\end"))) {
                m_i += 4;
                readRawGroup();
                break;
            }
            // Stray '}' or \right inside an environment: skip it.
            ++m_i;
        }
        while (!rows.empty() && rows.back().empty())
            rows.pop_back();
        if (!rows.empty() && rows.back().size() == 1) {
            auto* last = dynamic_cast<RowBox*>(rows.back().front().get());
            if (last && last->isEmpty() && rows.size() > 1)
                rows.pop_back();
        }
        const bool cases = env == QLatin1String("cases");
        BoxPtr grid = std::make_unique<MatrixBox>(std::move(rows), cellSize, cases);
        *kind = Kind::Inner;
        QChar left, right;
        if (env == QLatin1String("pmatrix")) {
            left = QLatin1Char('(');
            right = QLatin1Char(')');
        } else if (env == QLatin1String("bmatrix")) {
            left = QLatin1Char('[');
            right = QLatin1Char(']');
        } else if (env == QLatin1String("Bmatrix")) {
            left = QLatin1Char('{');
            right = QLatin1Char('}');
        } else if (env == QLatin1String("vmatrix")) {
            left = QLatin1Char('|');
            right = QLatin1Char('|');
        } else if (env == QLatin1String("Vmatrix")) {
            left = QChar(0x2016);
            right = QChar(0x2016);
        } else if (cases) {
            left = QLatin1Char('{');
            right = QLatin1Char('.');
        } else {
            return grid;
        }
        return std::make_unique<DelimBox>(left, std::move(grid), right, size);
    }

    BoxPtr parseCommand(qreal size, Kind* kind)
    {
        const QString cmd = readCommand();
        *kind = Kind::Ord;
        if (cmd.isEmpty())
            return nullptr;

        if (cmd == QLatin1String("frac") || cmd == QLatin1String("dfrac") || cmd == QLatin1String("tfrac")
            || cmd == QLatin1String("cfrac")) {
            const qreal inner = cmd == QLatin1String("tfrac") ? scriptSize(size) : std::max(size * 0.9, 6.0);
            BoxPtr num = parseArgument(inner);
            BoxPtr den = parseArgument(inner);
            *kind = Kind::Inner;
            return std::make_unique<FracBox>(std::move(num), std::move(den), size, true);
        }
        if (cmd == QLatin1String("binom")) {
            BoxPtr num = parseArgument(size * 0.9);
            BoxPtr den = parseArgument(size * 0.9);
            *kind = Kind::Inner;
            return std::make_unique<DelimBox>(QLatin1Char('('), std::make_unique<FracBox>(std::move(num), std::move(den), size, false),
                                              QLatin1Char(')'), size);
        }
        if (cmd == QLatin1String("sqrt")) {
            BoxPtr index;
            skipSpaces();
            if (!atEnd() && m_s[m_i] == QLatin1Char('[')) {
                ++m_i;
                const int start = m_i;
                int depth = 0;
                while (m_i < m_s.size() && !(m_s[m_i] == QLatin1Char(']') && depth == 0)) {
                    if (m_s[m_i] == QLatin1Char('{'))
                        ++depth;
                    else if (m_s[m_i] == QLatin1Char('}'))
                        --depth;
                    ++m_i;
                }
                const QString indexText = m_s.mid(start, m_i - start);
                if (m_i < m_s.size())
                    ++m_i;
                Parser sub(indexText, m_family);
                index = sub.parseAll(std::max(size * 0.5, 6.0));
            }
            BoxPtr body = parseArgument(size);
            return std::make_unique<RadicalBox>(std::move(body), std::move(index), size);
        }
        if (cmd == QLatin1String("left")) {
            const QChar left = readDelimiter();
            auto body = parseRow(size);
            QChar right = QLatin1Char('.');
            if (lookingAt(QStringLiteral("\\right"))) {
                m_i += 6;
                right = readDelimiter();
            }
            *kind = Kind::Inner;
            return std::make_unique<DelimBox>(left, std::move(body), right, size);
        }
        if (cmd == QLatin1String("right"))
            return nullptr;
        if (cmd == QLatin1String("begin"))
            return parseMatrix(readRawGroup(), size, kind);
        if (cmd == QLatin1String("end")) {
            readRawGroup();
            return nullptr;
        }
        if (cmd == QLatin1String("text") || cmd == QLatin1String("mathrm") || cmd == QLatin1String("textrm")
            || cmd == QLatin1String("operatorname") || cmd == QLatin1String("mbox")) {
            const QString text = readRawGroup();
            *kind = cmd == QLatin1String("operatorname") ? Kind::Op : Kind::Ord;
            return glyph(text, size, false, *kind);
        }
        if (cmd == QLatin1String("mathbf") || cmd == QLatin1String("textbf") || cmd == QLatin1String("boldsymbol")) {
            return glyph(readRawGroup(), size, false, Kind::Ord, true);
        }
        if (cmd == QLatin1String("mathit") || cmd == QLatin1String("textit"))
            return glyph(readRawGroup(), size, true, Kind::Ord);

        struct AccentName
        {
            const char* name;
            AccentKind kind;
        };
        static const AccentName accents[] = {
            {"vec", AccentKind::Vec},       {"overrightarrow", AccentKind::Vec}, {"hat", AccentKind::Hat},
            {"widehat", AccentKind::Hat},   {"bar", AccentKind::Bar},            {"overline", AccentKind::Bar},
            {"dot", AccentKind::Dot},       {"ddot", AccentKind::DDot},          {"tilde", AccentKind::Tilde},
            {"widetilde", AccentKind::Tilde}, {"underline", AccentKind::Underline},
        };
        for (const auto& a : accents) {
            if (cmd == QLatin1String(a.name)) {
                BoxPtr body = parseArgument(size);
                return std::make_unique<AccentBox>(std::move(body), a.kind, size);
            }
        }
        for (const auto& op : kBigOps) {
            if (cmd == QLatin1String(op.name)) {
                const bool integral = !op.limits;
                auto box = std::make_unique<BigGlyphBox>(QString(QChar(op.code)), font(size * (integral ? 1.75 : 1.45), false), size);
                box->limits = op.limits;
                *kind = Kind::Op;
                return box;
            }
        }
        for (const char* f : kLimitFunctions) {
            if (cmd == QLatin1String(f)) {
                auto g = std::make_unique<GlyphBox>(cmd, font(size, false), Kind::Op, size);
                g->limits = true;
                *kind = Kind::Op;
                return g;
            }
        }
        for (const char* f : kFunctions) {
            if (cmd == QLatin1String(f)) {
                *kind = Kind::Op;
                return glyph(cmd, size, false, Kind::Op);
            }
        }
        // Spacing commands.
        if (cmd == QLatin1String(","))
            return std::make_unique<SpaceBox>(size * 3.0 / 18.0);
        if (cmd == QLatin1String(":") || cmd == QLatin1String(">"))
            return std::make_unique<SpaceBox>(size * 4.0 / 18.0);
        if (cmd == QLatin1String(";"))
            return std::make_unique<SpaceBox>(size * 5.0 / 18.0);
        if (cmd == QLatin1String("!"))
            return std::make_unique<SpaceBox>(-size * 3.0 / 18.0);
        if (cmd == QLatin1String("quad"))
            return std::make_unique<SpaceBox>(size);
        if (cmd == QLatin1String("qquad"))
            return std::make_unique<SpaceBox>(size * 2);
        if (cmd == QLatin1String(" "))
            return std::make_unique<SpaceBox>(size * 0.33);

        if (const Symbol* s = findSymbol(cmd)) {
            *kind = s->kind;
            const bool lowerGreek = s->code >= 0x03B1 && s->code <= 0x03C9;
            return glyph(QString(QChar(s->code)), size, lowerGreek, s->kind);
        }
        // Unknown command: show it literally.
        return glyph(QLatin1Char('\\') + cmd, size, false, Kind::Ord);
    }

    const QString& m_s;
    QString m_family;
    int m_i = 0;
};

QString defaultMathFamily()
{
    static const QString family = []() {
        const QStringList preferred = {QStringLiteral("Cambria Math"), QStringLiteral("STIX Two Math"),
                                       QStringLiteral("STIXGeneral"), QStringLiteral("Latin Modern Math"),
                                       QStringLiteral("Cambria"), QStringLiteral("DejaVu Serif"),
                                       QStringLiteral("Times New Roman"), QStringLiteral("Liberation Serif")};
        const QStringList available = QFontDatabase().families();
        for (const QString& f : preferred)
            if (available.contains(f, Qt::CaseInsensitive))
                return f;
        return QFont().family();
    }();
    return family;
}

} // namespace

MathTypesetter::MathTypesetter(const QString& fontFamily)
    : m_family(fontFamily.isEmpty() ? defaultMathFamily() : fontFamily)
{
}

BoxPtr MathTypesetter::layout(const QString& latex, qreal pixelSize) const
{
    Parser parser(latex, m_family);
    return parser.parseAll(std::max(4.0, pixelSize));
}

} // namespace cb::mathtype
