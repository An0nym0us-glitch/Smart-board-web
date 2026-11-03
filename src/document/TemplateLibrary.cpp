#include "document/TemplateLibrary.h"

#include "document/ImageStore.h"
#include "storage/ProjectSerializer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace cb {

TemplateLibrary::TemplateLibrary()
{
    m_templates = fallbackBuiltins();
}

QVector<TemplateSpec> TemplateLibrary::fallbackBuiltins()
{
    auto make = [](const char* id, const char* name, TemplateKind kind, const char* bg, const char* line,
                   const char* accent, qreal spacing, int major) {
        TemplateSpec t;
        t.id = QString::fromLatin1(id);
        t.name = QString::fromLatin1(name);
        t.kind = kind;
        t.background = QColor(QString::fromLatin1(bg));
        t.lineColor = QColor(QString::fromLatin1(line));
        t.accentColor = QColor(QString::fromLatin1(accent));
        t.spacing = spacing;
        t.majorEvery = major;
        return t;
    };
    return {
        make("blackboard", "Blackboard", TemplateKind::Blank, "#1f2b26", "#28ffffff", "#5affffff", 40, 5),
        make("slate", "Slate", TemplateKind::Blank, "#1b1f24", "#28ffffff", "#5affffff", 40, 5),
        make("whiteboard", "Whiteboard", TemplateKind::Blank, "#f7f7f2", "#30000000", "#60000000", 40, 5),
        make("grid", "Grid", TemplateKind::Grid, "#1f2b26", "#22ffffff", "#44ffffff", 40, 5),
        make("dots", "Dot grid", TemplateKind::Dots, "#1f2b26", "#22ffffff", "#66ffffff", 40, 5),
        make("ruled", "Ruled paper", TemplateKind::Ruled, "#1f2b26", "#33ffffff", "#80ef9a9a", 56, 0),
        make("graph", "Graph paper", TemplateKind::GraphPaper, "#1f2b26", "#18ffffff", "#40ffffff", 8, 5),
        make("music", "Music staff", TemplateKind::MusicStaff, "#1f2b26", "#90ffffff", "#90ffffff", 14, 0),
        make("coordinate", "Coordinate plane", TemplateKind::CoordinatePlane, "#1f2b26", "#20ffffff", "#40ffffff", 40, 5),
        make("white-grid", "White grid", TemplateKind::Grid, "#f7f7f2", "#22000000", "#44000000", 40, 5),
    };
}

QString TemplateLibrary::customDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/templates");
}

void TemplateLibrary::load()
{
    QVector<TemplateSpec> builtins;
    QFile f(QStringLiteral(":/templates/builtin.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("templates")).toArray();
        for (const QJsonValue& v : arr)
            builtins.push_back(TemplateSpec::fromJson(v.toObject()));
    }
    if (builtins.isEmpty())
        builtins = fallbackBuiltins();

    m_custom.clear();
    QFile cf(customDir() + QStringLiteral("/custom.json"));
    if (cf.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(cf.readAll()).object().value(QStringLiteral("templates")).toArray();
        for (const QJsonValue& v : arr)
            m_custom.push_back(TemplateSpec::fromJson(v.toObject()));
    }
    m_templates = builtins + m_custom;
}

TemplateSpec TemplateLibrary::find(const QString& id) const
{
    for (const TemplateSpec& t : m_templates)
        if (t.id == id)
            return t;
    return m_templates.isEmpty() ? TemplateSpec() : m_templates.first();
}

bool TemplateLibrary::contains(const QString& id) const
{
    for (const TemplateSpec& t : m_templates)
        if (t.id == id)
            return true;
    return false;
}

bool TemplateLibrary::isCustom(const QString& id) const
{
    for (const TemplateSpec& t : m_custom)
        if (t.id == id)
            return true;
    return false;
}

void TemplateLibrary::saveCustom() const
{
    QDir().mkpath(customDir());
    QJsonArray arr;
    for (const TemplateSpec& t : m_custom)
        arr.append(t.toJson());
    QJsonObject root;
    root.insert(QStringLiteral("templates"), arr);
    ProjectSerializer::writeFileAtomically(customDir() + QStringLiteral("/custom.json"),
                                           QJsonDocument(root).toJson(QJsonDocument::Indented), nullptr);
}

bool TemplateLibrary::addCustom(TemplateSpec spec, const QByteArray& imageBytes, QString* error)
{
    QDir().mkpath(customDir());
    if (!imageBytes.isEmpty()) {
        const QString key = QString::fromLatin1(QCryptographicHash::hash(imageBytes, QCryptographicHash::Sha1).toHex());
        if (!ProjectSerializer::writeFileAtomically(customDir() + QLatin1Char('/') + key + QStringLiteral(".img"),
                                                    imageBytes, error))
            return false;
        spec.imageKey = key;
        spec.kind = TemplateKind::Image;
    }
    if (spec.id.isEmpty() || contains(spec.id))
        spec.id = QStringLiteral("custom-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m_custom.push_back(spec);
    saveCustom();
    load();
    return true;
}

bool TemplateLibrary::removeCustom(const QString& id)
{
    for (int i = 0; i < m_custom.size(); ++i) {
        if (m_custom[i].id == id) {
            m_custom.remove(i);
            saveCustom();
            load();
            return true;
        }
    }
    return false;
}

void TemplateLibrary::ensureImage(const TemplateSpec& spec, ImageStore& store) const
{
    if (spec.kind != TemplateKind::Image || spec.imageKey.isEmpty() || store.contains(spec.imageKey))
        return;
    QFile f(customDir() + QLatin1Char('/') + spec.imageKey + QStringLiteral(".img"));
    if (f.open(QIODevice::ReadOnly))
        store.addEncoded(f.readAll());
}

} // namespace cb
