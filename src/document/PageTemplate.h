#pragma once

#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace cb {

class ImageStore;
class CoordinateSystem;

enum class TemplateKind {
    Blank,
    Grid,
    Dots,
    Ruled,
    GraphPaper,
    MusicStaff,
    CoordinatePlane,
    Image,
};

QString templateKindName(TemplateKind kind);
TemplateKind templateKindFromName(const QString& name);

/// Background description of a page. Stored by value in each page so documents are self-contained;
/// the TemplateLibrary provides reusable presets.
struct TemplateSpec
{
    QString id = QStringLiteral("blackboard");
    QString name = QStringLiteral("Blackboard");
    TemplateKind kind = TemplateKind::Blank;
    QColor background = QColor(0x1f, 0x2b, 0x26);
    QColor lineColor = QColor(255, 255, 255, 40);
    QColor accentColor = QColor(255, 255, 255, 90);
    qreal spacing = 40.0;
    int majorEvery = 5;
    QString imageKey; ///< for TemplateKind::Image (asset in the document ImageStore)

    bool isDark() const { return background.lightnessF() < 0.5; }

    QJsonObject toJson() const;
    static TemplateSpec fromJson(const QJsonObject& obj);
    bool operator==(const TemplateSpec& other) const;
    bool operator!=(const TemplateSpec& other) const { return !(*this == other); }
};

/// Draws template backgrounds for any visible area with level-of-detail control.
class TemplateRenderer
{
public:
    /// painter must be in page coordinates. visible is the page area to fill.
    static void paint(QPainter& painter, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                      const ImageStore* images, const CoordinateSystem& coordinates, const QRectF& frame);

private:
    static void paintGrid(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                          const QPointF& anchor, qreal spacing, int majorEvery);
    static void paintDots(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                          const QPointF& anchor);
    static void paintRuled(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                           const QRectF& frame);
    static void paintMusic(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom);
    static void paintCoordinatePlane(QPainter& p, const TemplateSpec& spec, const QRectF& visible, qreal zoom,
                                     const CoordinateSystem& cs);
};

} // namespace cb
