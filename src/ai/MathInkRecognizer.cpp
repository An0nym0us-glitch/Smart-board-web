#include "ai/MathInkRecognizer.h"

#include "core/Geometry.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cb {

namespace {

// ============================================================================ $P point-cloud matcher

constexpr int kCloudSize = 32;

struct CloudPoint
{
    double x = 0.0;
    double y = 0.0;
};
using Cloud = std::vector<CloudPoint>;

double pathLength(const QVector<QVector<QPointF>>& strokes)
{
    double len = 0.0;
    for (const auto& s : strokes)
        for (int i = 1; i < s.size(); ++i)
            len += geom::distance(s[i - 1], s[i]);
    return len;
}

/// Resamples all strokes to n equidistant points (jumps between strokes are not part of the path).
Cloud resample(const QVector<QVector<QPointF>>& strokes, int n)
{
    Cloud out;
    const double total = pathLength(strokes);
    if (total <= 1e-9) {
        // A dot: n copies of the point.
        const QPointF p = strokes.isEmpty() || strokes.first().isEmpty() ? QPointF() : strokes.first().first();
        out.assign(static_cast<size_t>(n), {p.x(), p.y()});
        return out;
    }
    const double interval = total / (n - 1);
    double acc = 0.0;
    for (const auto& stroke : strokes) {
        if (stroke.isEmpty())
            continue;
        if (out.empty())
            out.push_back({stroke.first().x(), stroke.first().y()});
        QPointF prev = stroke.first();
        for (int i = 1; i < stroke.size(); ++i) {
            QPointF cur = stroke[i];
            double d = geom::distance(prev, cur);
            while (acc + d >= interval && d > 0.0) {
                const double t = (interval - acc) / d;
                const QPointF q = prev + (cur - prev) * t;
                out.push_back({q.x(), q.y()});
                prev = q;
                d = geom::distance(prev, cur);
                acc = 0.0;
                if (static_cast<int>(out.size()) >= n)
                    break;
            }
            acc += d;
            prev = cur;
            if (static_cast<int>(out.size()) >= n)
                break;
        }
    }
    const QPointF last = strokes.last().isEmpty() ? QPointF(out.back().x, out.back().y) : strokes.last().last();
    while (static_cast<int>(out.size()) < n)
        out.push_back({last.x(), last.y()});
    out.resize(static_cast<size_t>(n));
    return out;
}

/// Uniform scale into the unit box (aspect ratio preserved) and centroid at the origin.
void normalize(Cloud& cloud)
{
    double minX = std::numeric_limits<double>::max(), minY = minX;
    double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
    for (const CloudPoint& p : cloud) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const double size = std::max({maxX - minX, maxY - minY, 1e-9});
    double cx = 0.0, cy = 0.0;
    for (CloudPoint& p : cloud) {
        p.x = (p.x - minX) / size;
        p.y = (p.y - minY) / size;
        cx += p.x;
        cy += p.y;
    }
    cx /= cloud.size();
    cy /= cloud.size();
    for (CloudPoint& p : cloud) {
        p.x -= cx;
        p.y -= cy;
    }
}

Cloud makeCloud(const QVector<QVector<QPointF>>& strokes)
{
    Cloud c = resample(strokes, kCloudSize);
    normalize(c);
    return c;
}

double cloudDistance(const Cloud& a, const Cloud& b, int start, double bound)
{
    const int n = static_cast<int>(a.size());
    std::vector<bool> matched(static_cast<size_t>(n), false);
    double sum = 0.0;
    int i = start;
    do {
        double best = std::numeric_limits<double>::max();
        int index = -1;
        for (int j = 0; j < n; ++j) {
            if (matched[static_cast<size_t>(j)])
                continue;
            const double dx = a[static_cast<size_t>(i)].x - b[static_cast<size_t>(j)].x;
            const double dy = a[static_cast<size_t>(i)].y - b[static_cast<size_t>(j)].y;
            const double d = std::sqrt(dx * dx + dy * dy);
            if (d < best) {
                best = d;
                index = j;
            }
        }
        matched[static_cast<size_t>(index)] = true;
        const double weight = 1.0 - double((i - start + n) % n) / n;
        sum += weight * best;
        if (sum >= bound)
            return sum; // cannot beat the best match found so far
        i = (i + 1) % n;
    } while (i != start);
    return sum;
}

double greedyMatch(const Cloud& a, const Cloud& b, double bound)
{
    const int n = static_cast<int>(a.size());
    const int step = std::max(1, static_cast<int>(std::floor(std::pow(n, 0.5))));
    double best = bound;
    for (int i = 0; i < n; i += step) {
        best = std::min(best, cloudDistance(a, b, i, best));
        best = std::min(best, cloudDistance(b, a, i, best));
    }
    return best;
}

/// Ink length relative to the size of the symbol: a scribble has far more ink than any digit.
double inkRatio(const QVector<QVector<QPointF>>& strokes)
{
    QRectF box;
    double len = 0.0;
    for (const auto& s : strokes) {
        if (s.isEmpty())
            continue;
        const QRectF b = geom::boundingRect(s);
        box = box.isNull() ? b : box.united(b);
        for (int i = 1; i < s.size(); ++i)
            len += geom::distance(s[i - 1], s[i]);
    }
    const double diag = std::hypot(box.width(), box.height());
    return diag > 1e-9 ? len / diag : 0.0;
}

// ================================================================================== Templates

using Strokes = QVector<QVector<QPointF>>;

struct Template
{
    QString symbol;
    Cloud cloud;
    double inkRatio = 1.0;
};

QVector<QPointF> poly(std::initializer_list<QPointF> pts)
{
    return QVector<QPointF>(pts);
}

/// Smooth curve through control points (Catmull-Rom), so templates look like handwriting.
QVector<QPointF> curve(std::initializer_list<QPointF> control)
{
    const QVector<QPointF> c(control);
    if (c.size() < 3)
        return c;
    QVector<QPointF> out;
    for (int i = 0; i + 1 < c.size(); ++i) {
        const QPointF p0 = c[std::max(0, i - 1)];
        const QPointF p1 = c[i];
        const QPointF p2 = c[i + 1];
        const QPointF p3 = c[std::min(c.size() - 1, i + 2)];
        for (int k = 0; k < 8; ++k) {
            const double t = k / 8.0;
            const double t2 = t * t;
            const double t3 = t2 * t;
            out.push_back(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3));
        }
    }
    out.push_back(c.last());
    return out;
}

QVector<QPointF> ellipse(QPointF center, double rx, double ry, double startDeg, double sweepDeg)
{
    QVector<QPointF> out;
    const int n = 36;
    for (int i = 0; i <= n; ++i) {
        const double a = geom::degToRad(startDeg + sweepDeg * i / n);
        out.push_back(center + QPointF(rx * std::cos(a), -ry * std::sin(a)));
    }
    return out;
}

const QVector<Template>& templates()
{
    static const QVector<Template> list = []() {
        QVector<QPair<QString, Strokes>> defs;
        auto add = [&](const QString& s, const Strokes& strokes) { defs.push_back({s, strokes}); };
        // Digits (0..100 box, y down).
        add(QStringLiteral("0"), {ellipse({50, 50}, 32, 48, 90, 360)});
        add(QStringLiteral("0"), {ellipse({50, 50}, 26, 48, 80, -360)});
        add(QStringLiteral("1"), {poly({{50, 0}, {50, 100}})});
        add(QStringLiteral("1"), {poly({{28, 22}, {50, 0}, {50, 100}})});
        add(QStringLiteral("1"), {poly({{28, 22}, {50, 0}, {50, 100}}), poly({{28, 100}, {72, 100}})});
        add(QStringLiteral("2"), {curve({{18, 28}, {30, 6}, {52, 0}, {74, 8}, {80, 28}, {70, 50}, {45, 72}, {18, 100}}), poly({{18, 100}, {86, 100}})});
        add(QStringLiteral("2"), {curve({{18, 28}, {30, 6}, {52, 0}, {74, 8}, {80, 28}, {70, 50}, {45, 72}, {18, 100}, {50, 100}, {86, 100}})});
        add(QStringLiteral("3"), {curve({{20, 12}, {48, 0}, {76, 10}, {76, 32}, {48, 48}, {78, 60}, {82, 84}, {56, 100}, {20, 90}})});
        add(QStringLiteral("3"), {curve({{22, 6}, {50, 0}, {72, 14}, {62, 36}, {44, 46}}), curve({{44, 46}, {74, 58}, {80, 82}, {54, 100}, {22, 94}})});
        add(QStringLiteral("4"), {poly({{62, 0}, {14, 66}, {86, 66}}), poly({{66, 22}, {66, 100}})});
        add(QStringLiteral("4"), {poly({{20, 0}, {16, 60}, {86, 60}}), poly({{66, 0}, {66, 100}})});
        add(QStringLiteral("4"), {poly({{62, 0}, {14, 66}, {86, 66}, {66, 66}, {66, 22}, {66, 100}})});
        add(QStringLiteral("5"), {poly({{78, 0}, {26, 0}}), curve({{26, 0}, {22, 44}, {50, 38}, {78, 52}, {80, 80}, {56, 100}, {20, 92}})});
        add(QStringLiteral("5"), {curve({{26, 0}, {22, 44}, {50, 38}, {78, 52}, {80, 80}, {56, 100}, {20, 92}}), poly({{26, 0}, {78, 0}})});
        add(QStringLiteral("5"), {curve({{78, 0}, {26, 0}, {22, 44}, {50, 38}, {78, 52}, {80, 80}, {56, 100}, {20, 92}})});
        add(QStringLiteral("6"), {curve({{72, 4}, {44, 0}, {24, 22}, {16, 56}, {22, 86}, {46, 100}, {72, 90}, {76, 66}, {54, 50}, {30, 54}, {18, 70}})});
        add(QStringLiteral("7"), {poly({{14, 0}, {86, 0}, {38, 100}})});
        add(QStringLiteral("7"), {poly({{14, 0}, {86, 0}, {38, 100}}), poly({{36, 50}, {76, 50}})});
        add(QStringLiteral("8"), {curve({{68, 12}, {50, 0}, {28, 10}, {30, 32}, {50, 50}, {72, 68}, {70, 90}, {50, 100}, {28, 90}, {30, 68}, {50, 50}, {70, 32}, {68, 12}})});
        add(QStringLiteral("8"), {ellipse({50, 25}, 22, 25, 90, 360), ellipse({50, 75}, 26, 25, 90, 360)});
        add(QStringLiteral("9"), {curve({{76, 20}, {56, 0}, {30, 4}, {22, 24}, {34, 44}, {60, 42}, {76, 20}}), poly({{76, 20}, {74, 60}, {70, 100}})});
        add(QStringLiteral("9"), {curve({{76, 20}, {56, 0}, {30, 4}, {22, 24}, {34, 44}, {60, 42}, {76, 20}, {76, 60}, {66, 92}, {44, 100}})});
        // Letters (x-height box).
        add(QStringLiteral("x"), {poly({{15, 15}, {85, 85}}), poly({{85, 15}, {15, 85}})});
        add(QStringLiteral("x"), {curve({{10, 20}, {30, 15}, {50, 50}, {70, 85}, {90, 80}}), poly({{85, 15}, {15, 85}})});
        add(QStringLiteral("x"), {poly({{25, 0}, {75, 100}}), poly({{75, 0}, {25, 100}})});
        add(QStringLiteral("x"), {poly({{0, 20}, {100, 80}}), poly({{100, 20}, {0, 80}})});
        add(QStringLiteral("y"), {poly({{20, 0}, {52, 56}}), poly({{82, 0}, {30, 100}})});
        add(QStringLiteral("y"), {curve({{20, 0}, {24, 36}, {48, 50}, {74, 38}, {78, 0}, {76, 70}, {58, 100}, {30, 94}})});
        add(QStringLiteral("a"), {curve({{74, 24}, {50, 8}, {24, 24}, {18, 56}, {40, 82}, {66, 74}, {74, 44}}), poly({{76, 14}, {78, 86}})});
        add(QStringLiteral("a"), {curve({{74, 24}, {50, 8}, {24, 24}, {18, 56}, {40, 82}, {66, 74}, {74, 44}, {75, 14}, {77, 60}, {82, 86}})});
        add(QStringLiteral("b"), {poly({{26, 0}, {26, 100}}), curve({{26, 62}, {50, 44}, {76, 58}, {78, 84}, {52, 100}, {26, 94}})});
        add(QStringLiteral("b"), {curve({{26, 0}, {26, 50}, {26, 100}, {30, 72}, {54, 54}, {78, 68}, {70, 96}, {40, 98}, {26, 92}})});
        add(QStringLiteral("c"), {curve({{78, 20}, {56, 4}, {28, 14}, {16, 46}, {26, 78}, {52, 92}, {80, 80}})});
        add(QStringLiteral("n"), {curve({{20, 20}, {20, 60}, {20, 100}, {22, 50}, {42, 26}, {66, 26}, {78, 46}, {80, 100}})});
        add(QStringLiteral("n"), {poly({{20, 20}, {20, 100}}), curve({{20, 50}, {42, 26}, {66, 26}, {78, 46}, {80, 100}})});
        // Operators and brackets.
        add(QStringLiteral("+"), {poly({{50, 8}, {50, 92}}), poly({{8, 50}, {92, 50}})});
        add(QStringLiteral("("), {curve({{66, 0}, {42, 22}, {32, 50}, {42, 78}, {66, 100}})});
        add(QStringLiteral("("), {curve({{58, 0}, {46, 24}, {42, 50}, {46, 76}, {58, 100}})});
        add(QStringLiteral(")"), {curve({{34, 0}, {58, 22}, {68, 50}, {58, 78}, {34, 100}})});
        add(QStringLiteral(")"), {curve({{42, 0}, {54, 24}, {58, 50}, {54, 76}, {42, 100}})});
        // Every template also in a narrower and a wider variant: handwriting varies in aspect
        // ratio, and the matcher keeps aspect ratios.
        QVector<Template> out;
        for (const auto& d : defs) {
            for (double sx : {1.0, 0.75, 1.3}) {
                Strokes variant = d.second;
                for (auto& stroke : variant)
                    for (QPointF& p : stroke)
                        p.setX(50.0 + (p.x() - 50.0) * sx);
                out.push_back({d.first, makeCloud(variant), inkRatio(variant)});
            }
        }
        return out;
    }();
    return list;
}

// ===================================================================================== Layout

struct StrokeInfo
{
    QVector<QPointF> points;
    QRectF box;
    double length = 0.0;
};

double polylineLength(const QVector<QPointF>& pts)
{
    double len = 0.0;
    for (int i = 1; i < pts.size(); ++i)
        len += geom::distance(pts[i - 1], pts[i]);
    return len;
}

/// A flat, straight stroke: minus sign, fraction bar or half of "=" (a flat zig-zag is not).
bool isBar(const StrokeInfo& s)
{
    return s.box.width() > 2.5 * std::max(s.box.height(), 1e-6) && s.length < 1.3 * std::hypot(s.box.width(), s.box.height()) + 1e-6;
}

QRectF boxOf(const QVector<QPointF>& pts)
{
    return geom::boundingRect(pts);
}

bool isFlat(const QRectF& r)
{
    return r.width() > 2.5 * std::max(r.height(), 1e-6);
}

bool strokesCross(const QVector<QPointF>& a, const QVector<QPointF>& b)
{
    for (int i = 1; i < a.size(); ++i) {
        const QLineF la(a[i - 1], a[i]);
        for (int j = 1; j < b.size(); ++j) {
            QPointF hit;
            if (la.intersects(QLineF(b[j - 1], b[j]), &hit) == QLineF::BoundedIntersection)
                return true;
        }
    }
    return false;
}

double overlapRatio(const QRectF& a, const QRectF& b)
{
    const double inter = std::min(a.right(), b.right()) - std::max(a.left(), b.left());
    const double narrow = std::max(1e-6, std::min(a.width(), b.width()));
    return inter / narrow;
}

struct Item
{
    QString latex;
    QRectF box;
    double score = 1.0;
    bool base = true; ///< digit, letter, bracket or fraction (can carry a superscript)
};

struct Context
{
    const MathInkRecognizer* recognizer;
    double minScore;
    bool relaxed;
};

QString recognizeRegion(const QVector<StrokeInfo>& strokes, const Context& ctx, double* confidence);

/// Groups the strokes into items (symbols, fractions, operators) sorted left to right.
QVector<Item> buildItems(QVector<StrokeInfo> strokes, const Context& ctx, double* confidence)
{
    QVector<Item> items;
    if (strokes.isEmpty())
        return items;
    QRectF all;
    for (const auto& s : strokes)
        all = all.isNull() ? s.box : all.united(s.box);
    double height = 0.0;
    int counted = 0;
    for (const auto& s : strokes) {
        if (!isFlat(s.box)) {
            height += s.box.height();
            ++counted;
        }
    }
    height = counted > 0 ? height / counted : all.height();
    height = std::max(height, 1.0);

    // 1. Fraction bars: flat strokes with ink above and below, widest first.
    std::vector<int> order(static_cast<size_t>(strokes.size()));
    for (int i = 0; i < strokes.size(); ++i)
        order[static_cast<size_t>(i)] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return strokes[a].box.width() > strokes[b].box.width(); });
    for (int bar : order) {
        if (bar >= strokes.size())
            continue;
        const QRectF b = strokes[bar].box;
        if (!isBar(strokes[bar]) || b.width() < 0.6 * height)
            continue;
        QVector<StrokeInfo> above, below, rest;
        const double margin = 0.1 * b.width();
        for (int i = 0; i < strokes.size(); ++i) {
            if (i == bar)
                continue;
            const QRectF r = strokes[i].box;
            const bool within = r.center().x() >= b.left() - margin && r.center().x() <= b.right() + margin;
            if (within && r.bottom() <= b.center().y() + 0.1 * height)
                above.push_back(strokes[i]);
            else if (within && r.top() >= b.center().y() - 0.1 * height)
                below.push_back(strokes[i]);
            else
                rest.push_back(strokes[i]);
        }
        // An equals sign is two similar bars close together, not a fraction.
        const bool equalsLike = (above.size() == 1 && isBar(above.first()) && below.isEmpty())
            || (below.size() == 1 && isBar(below.first()) && above.isEmpty());
        if (above.isEmpty() || below.isEmpty() || equalsLike)
            continue;
        double c1 = 1.0, c2 = 1.0;
        const QString num = recognizeRegion(above, ctx, &c1);
        const QString den = recognizeRegion(below, ctx, &c2);
        Item frac;
        frac.latex = QStringLiteral("\\frac{%1}{%2}").arg(num, den);
        frac.box = b;
        for (const auto& s : above)
            frac.box = frac.box.united(s.box);
        for (const auto& s : below)
            frac.box = frac.box.united(s.box);
        frac.score = std::min(c1, c2);
        items = buildItems(rest, ctx, confidence);
        items.push_back(frac);
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.box.center().x() < b.box.center().x(); });
        if (confidence)
            *confidence = std::min(*confidence, frac.score);
        return items;
    }

    // 2. Group strokes into symbols: crossing or strongly overlapping (in x) strokes belong together.
    std::sort(strokes.begin(), strokes.end(), [](const StrokeInfo& a, const StrokeInfo& b) { return a.box.left() < b.box.left(); });
    QVector<QVector<StrokeInfo>> groups;
    QVector<QRectF> boxes;
    for (const StrokeInfo& s : strokes) {
        int target = -1;
        for (int g = 0; g < groups.size(); ++g) {
            const bool flatPair = isBar(s) && std::all_of(groups[g].begin(), groups[g].end(), [](const StrokeInfo& o) { return isBar(o); });
            const double overlap = overlapRatio(s.box, boxes[g]);
            bool cross = false;
            for (const StrokeInfo& o : groups[g])
                cross = cross || strokesCross(s.points, o.points);
            // Two flat strokes form "=" only when stacked closely.
            if (flatPair && overlap > 0.4 && std::abs(s.box.center().y() - boxes[g].center().y()) < 0.9 * height) {
                target = g;
                break;
            }
            if (!flatPair && (cross || overlap > 0.55)) {
                target = g;
                break;
            }
        }
        if (target < 0) {
            groups.push_back({s});
            boxes.push_back(s.box);
        } else {
            groups[target].push_back(s);
            boxes[target] = boxes[target].united(s.box);
        }
    }

    // 3. Classify each group.
    for (int g = 0; g < groups.size(); ++g) {
        const auto& group = groups[g];
        const QRectF box = boxes[g];
        Item item;
        item.box = box;
        if (std::max(box.width(), box.height()) < 0.18 * height) {
            item.latex = QStringLiteral(".");
            item.base = false;
        } else if (group.size() == 1 && isBar(group[0])) {
            item.latex = QStringLiteral("-");
            item.base = false;
        } else if (group.size() == 2 && isBar(group[0]) && isBar(group[1])) {
            item.latex = QStringLiteral("=");
            item.base = false;
        } else {
            Strokes ink;
            for (const StrokeInfo& s : group)
                ink.push_back(s.points);
            double score = 0.0;
            item.latex = ctx.recognizer->classifySymbol(ink, &score);
            item.score = score;
            item.base = item.latex != QLatin1String("+") && item.latex != QLatin1String("(");
        }
        items.push_back(item);
        if (confidence)
            *confidence = std::min(*confidence, item.score);
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.box.center().x() < b.box.center().x(); });
    return items;
}

QString layoutItems(const QVector<Item>& items)
{
    // Line height: the tallest symbol that is not a raised script (median of the tall half).
    QVector<double> heights;
    for (const Item& it : items)
        if (it.base)
            heights.push_back(it.box.height());
    std::sort(heights.begin(), heights.end());
    const double lineHeight = heights.isEmpty() ? 1.0 : heights[heights.size() * 3 / 4 < heights.size() ? heights.size() * 3 / 4 : heights.size() - 1];
    // Baseline: the median bottom of the symbols that sit on the line.
    QVector<double> bottoms;
    for (const Item& it : items)
        if (it.base)
            bottoms.push_back(it.box.bottom());
    std::sort(bottoms.begin(), bottoms.end());
    const double baseline = bottoms.isEmpty() ? 0.0 : bottoms[bottoms.size() / 2];
    QString out;
    int i = 0;
    while (i < items.size()) {
        const Item& base = items[i];
        out += base.latex;
        ++i;
        if (!base.base)
            continue;
        // Superscript: smaller symbols raised above the base's middle.
        QString sup;
        while (i < items.size()) {
            const Item& it = items[i];
            // Raised: it ends well above the baseline and higher than the base symbol's middle
            // (operators such as "+", "-" and "=" sit in the middle of the line and are never
            // exponents); smaller than the line height.
            const bool raised = it.box.bottom() < baseline - 0.45 * lineHeight
                && it.box.center().y() < base.box.center().y() - 0.2 * base.box.height();
            const bool smaller = it.box.height() < 0.8 * lineHeight;
            const bool after = it.box.left() > base.box.left() + 0.3 * base.box.width();
            const bool high = it.box.bottom() < base.box.top() + 0.3 * base.box.height();
            if (!(raised && (smaller || high) && after))
                break;
            sup += it.latex;
            ++i;
        }
        if (!sup.isEmpty())
            out += QStringLiteral("^{") + sup + QLatin1Char('}');
    }
    return out;
}

QString recognizeRegion(const QVector<StrokeInfo>& strokes, const Context& ctx, double* confidence)
{
    double c = 1.0;
    const QVector<Item> items = buildItems(strokes, ctx, &c);
    if (confidence)
        *confidence = c;
    return layoutItems(items);
}

} // namespace

QStringList MathInkRecognizer::supportedSymbols()
{
    QStringList out = {QStringLiteral("-"), QStringLiteral("="), QStringLiteral(".")};
    for (const Template& t : templates())
        if (!out.contains(t.symbol))
            out << t.symbol;
    return out;
}

QString MathInkRecognizer::classifySymbol(const QVector<QVector<QPointF>>& strokes, double* score) const
{
    const Cloud cloud = makeCloud(strokes);
    double best = std::numeric_limits<double>::max();
    QString symbol;
    const Template* match = nullptr;
    for (const Template& t : templates()) {
        const double d = greedyMatch(cloud, t.cloud, best);
        if (d < best) {
            best = d;
            symbol = t.symbol;
            match = &t;
        }
    }
    // Shape can match while the amount of ink cannot (a zig-zag scribble looks like a wide "x" as
    // a point cloud): penalise ink length that is far from the matched template's.
    double inkFactor = 1.0;
    if (match && match->inkRatio > 0.0) {
        const double r = inkRatio(strokes);
        const double f = std::min(r / match->inkRatio, match->inkRatio / std::max(r, 1e-9));
        if (f < 0.6)
            inkFactor = f;
    }
    // Map the $P distance (0 = identical) to a 0..1 score. Clean handwriting of a known symbol
    // lands around 0.7..0.95, scribbles and unknown shapes below 0.55.
    constexpr double kWorst = 3.5;
    if (score)
        *score = std::clamp(1.0 - best / kWorst, 0.0, 1.0) * inkFactor;
    return symbol;
}

QVector<EquationCandidate> MathInkRecognizer::recognize(const InkSample& ink) const
{
    QVector<StrokeInfo> strokes;
    for (const auto& s : ink.strokes) {
        if (s.isEmpty())
            continue;
        strokes.push_back({s, boxOf(s), polylineLength(s)});
    }
    if (strokes.isEmpty())
        return {};
    Context ctx{this, m_options.minScore, m_options.relaxed};
    double confidence = 1.0;
    const QString latex = recognizeRegion(strokes, ctx, &confidence);
    if (latex.isEmpty())
        return {};
    if (!m_options.relaxed && confidence < m_options.minScore)
        return {};
    return {EquationCandidate{latex, confidence}};
}

} // namespace cb
