#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

namespace cb {

class ToolSettings;

/// Persistent application preferences (INI file in the user's AppData folder, portable and
/// human readable). Also persists the tool settings between sessions.
class AppSettings : public QObject
{
    Q_OBJECT
public:
    explicit AppSettings(QObject* parent = nullptr);

    /// 0 = automatic (derived from the screen).
    qreal uiScale() const { return m_uiScale; }
    void setUiScale(qreal scale);
    /// Size of the bottom ribbon relative to the interface size (1 = normal).
    qreal ribbonScale() const { return m_ribbonScale; }
    void setRibbonScale(qreal scale);
    int autosaveSeconds() const { return m_autosaveSeconds; }
    void setAutosaveSeconds(int seconds);
    bool palmErase() const { return m_palmErase; }
    void setPalmErase(bool on);
    bool multiUserTouch() const { return m_multiUserTouch; }
    void setMultiUserTouch(bool on);
    bool showPageFrame() const { return m_showPageFrame; }
    void setShowPageFrame(bool on);
    QString defaultTemplateId() const { return m_defaultTemplate; }
    void setDefaultTemplateId(const QString& id);
    QString lastDirectory() const { return m_lastDirectory; }
    void setLastDirectory(const QString& dir);
    QStringList recentFiles() const { return m_recent; }
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);

    void loadTools(ToolSettings& tools) const;
    void saveTools(const ToolSettings& tools);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    bool fullScreen() const;
    void setFullScreen(bool on);

    void sync() { m_store.sync(); }

signals:
    void changed();

private:
    void store(const QString& key, const QVariant& value);

    QSettings m_store;
    qreal m_uiScale = 0.0;
    qreal m_ribbonScale = 1.0;
    int m_autosaveSeconds = 60;
    bool m_palmErase = true;
    bool m_multiUserTouch = false;
    bool m_showPageFrame = false;
    QString m_defaultTemplate = QStringLiteral("blackboard");
    QString m_lastDirectory;
    QStringList m_recent;
};

} // namespace cb
