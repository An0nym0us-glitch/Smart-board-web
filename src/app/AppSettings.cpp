#include "app/AppSettings.h"

#include "tools/ToolSettings.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace cb {

namespace {
QString settingsPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/classboard.ini");
}
} // namespace

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
    , m_store(settingsPath(), QSettings::IniFormat)
{
    m_uiScale = m_store.value(QStringLiteral("ui/scale"), 0.0).toDouble();
    m_ribbonScale = std::clamp(m_store.value(QStringLiteral("ui/ribbonScale"), 1.0).toDouble(), 0.6, 1.5);
    m_autosaveSeconds = m_store.value(QStringLiteral("autosave/seconds"), 60).toInt();
    m_palmErase = m_store.value(QStringLiteral("input/palmErase"), true).toBool();
    m_multiUserTouch = m_store.value(QStringLiteral("input/multiUserTouch"), false).toBool();
    m_showPageFrame = m_store.value(QStringLiteral("view/showPageFrame"), false).toBool();
    m_defaultTemplate = m_store.value(QStringLiteral("lesson/defaultTemplate"), QStringLiteral("blackboard")).toString();
    m_lastDirectory = m_store.value(QStringLiteral("files/lastDirectory")).toString();
    m_recent = m_store.value(QStringLiteral("files/recent")).toStringList();
}

void AppSettings::store(const QString& key, const QVariant& value)
{
    m_store.setValue(key, value);
    emit changed();
}

void AppSettings::setUiScale(qreal scale)
{
    m_uiScale = scale <= 0 ? 0.0 : std::clamp(scale, 0.75, 3.0);
    store(QStringLiteral("ui/scale"), m_uiScale);
}

void AppSettings::setRibbonScale(qreal scale)
{
    m_ribbonScale = std::clamp(scale, 0.6, 1.5);
    store(QStringLiteral("ui/ribbonScale"), m_ribbonScale);
}

void AppSettings::setAutosaveSeconds(int seconds)
{
    m_autosaveSeconds = std::clamp(seconds, 15, 900);
    store(QStringLiteral("autosave/seconds"), m_autosaveSeconds);
}

void AppSettings::setPalmErase(bool on)
{
    m_palmErase = on;
    store(QStringLiteral("input/palmErase"), on);
}

void AppSettings::setMultiUserTouch(bool on)
{
    m_multiUserTouch = on;
    store(QStringLiteral("input/multiUserTouch"), on);
}

void AppSettings::setShowPageFrame(bool on)
{
    m_showPageFrame = on;
    store(QStringLiteral("view/showPageFrame"), on);
}

void AppSettings::setDefaultTemplateId(const QString& id)
{
    m_defaultTemplate = id;
    store(QStringLiteral("lesson/defaultTemplate"), id);
}

void AppSettings::setLastDirectory(const QString& dir)
{
    m_lastDirectory = dir;
    store(QStringLiteral("files/lastDirectory"), dir);
}

void AppSettings::addRecentFile(const QString& path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    m_recent.removeAll(abs);
    m_recent.prepend(abs);
    while (m_recent.size() > 8)
        m_recent.removeLast();
    store(QStringLiteral("files/recent"), m_recent);
}

void AppSettings::removeRecentFile(const QString& path)
{
    m_recent.removeAll(path);
    store(QStringLiteral("files/recent"), m_recent);
}

void AppSettings::loadTools(ToolSettings& t) const
{
    const QColor color(m_store.value(QStringLiteral("pen/color"), t.penColor().name()).toString());
    if (color.isValid())
        t.setPenColor(color);
    const StrokeStyle style = t.penStyle();
    t.setPenStyle(StrokeStyle::Pen);
    t.setPenWidth(m_store.value(QStringLiteral("pen/width"), t.penWidth()).toDouble());
    t.setPenStyle(StrokeStyle::Highlighter);
    t.setPenWidth(m_store.value(QStringLiteral("pen/highlighterWidth"), t.penWidth()).toDouble());
    t.setPenStyle(strokeStyleFromName(m_store.value(QStringLiteral("pen/style"), strokeStyleName(style)).toString()));
    t.setPressureEnabled(m_store.value(QStringLiteral("pen/pressure"), t.pressureEnabled()).toBool());
    t.setSmoothing(m_store.value(QStringLiteral("pen/smoothing"), t.smoothing()).toDouble());
    t.setShapeRecognition(m_store.value(QStringLiteral("pen/shapeRecognition"), t.shapeRecognition()).toBool());
    t.setEraserMode(static_cast<EraserMode>(m_store.value(QStringLiteral("eraser/mode"), int(t.eraserMode())).toInt()));
    t.setEraserSize(m_store.value(QStringLiteral("eraser/size"), t.eraserSize()).toDouble());
    t.setShowCoordinates(m_store.value(QStringLiteral("geometry/showCoordinates"), t.showCoordinates()).toBool());
    t.setSnapToGrid(m_store.value(QStringLiteral("geometry/snap"), t.snapToGrid()).toBool());
    TextFormat f = t.textFormat();
    f.pixelSize = m_store.value(QStringLiteral("text/size"), f.pixelSize).toInt();
    t.setTextFormat(f);
}

void AppSettings::saveTools(const ToolSettings& t)
{
    m_store.setValue(QStringLiteral("pen/color"), t.penColor().name());
    m_store.setValue(QStringLiteral("pen/style"), strokeStyleName(t.penStyle()));
    m_store.setValue(QStringLiteral("pen/pressure"), t.pressureEnabled());
    m_store.setValue(QStringLiteral("pen/smoothing"), t.smoothing());
    m_store.setValue(QStringLiteral("pen/shapeRecognition"), t.shapeRecognition());
    if (t.usesHighlighterWidth())
        m_store.setValue(QStringLiteral("pen/highlighterWidth"), t.penWidth());
    else
        m_store.setValue(QStringLiteral("pen/width"), t.penWidth());
    m_store.setValue(QStringLiteral("eraser/mode"), int(t.eraserMode()));
    m_store.setValue(QStringLiteral("eraser/size"), t.eraserSize());
    m_store.setValue(QStringLiteral("geometry/showCoordinates"), t.showCoordinates());
    m_store.setValue(QStringLiteral("geometry/snap"), t.snapToGrid());
    m_store.setValue(QStringLiteral("text/size"), t.textFormat().pixelSize);
}

QByteArray AppSettings::windowGeometry() const
{
    return m_store.value(QStringLiteral("window/geometry")).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray& geometry)
{
    m_store.setValue(QStringLiteral("window/geometry"), geometry);
}

bool AppSettings::fullScreen() const
{
    return m_store.value(QStringLiteral("window/fullScreen"), false).toBool();
}

void AppSettings::setFullScreen(bool on)
{
    m_store.setValue(QStringLiteral("window/fullScreen"), on);
}

} // namespace cb
