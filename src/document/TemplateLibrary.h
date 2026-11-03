#pragma once

#include "document/PageTemplate.h"

#include <QVector>

namespace cb {

class ImageStore;

/// Reusable page templates: built-in presets (resources/templates/builtin.json) plus custom
/// templates saved by the teacher (stored in the user's data folder, available in every lesson).
class TemplateLibrary
{
public:
    TemplateLibrary();

    /// Loads built-in and custom templates.
    void load();

    const QVector<TemplateSpec>& templates() const { return m_templates; }
    TemplateSpec find(const QString& id) const;
    bool contains(const QString& id) const;
    bool isCustom(const QString& id) const;

    /// Saves a custom template. imageBytes (optional) is the encoded background image.
    bool addCustom(TemplateSpec spec, const QByteArray& imageBytes, QString* error = nullptr);
    bool removeCustom(const QString& id);

    /// Makes sure the image of an image template is present in a document's store.
    void ensureImage(const TemplateSpec& spec, ImageStore& store) const;

    static QVector<TemplateSpec> fallbackBuiltins();

private:
    QString customDir() const;
    void saveCustom() const;

    QVector<TemplateSpec> m_templates;
    QVector<TemplateSpec> m_custom;
};

} // namespace cb
