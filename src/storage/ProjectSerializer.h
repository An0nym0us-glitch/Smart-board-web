#pragma once

#include "document/Document.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace cb {

/// Reads and writes native .classboard lesson files.
///
/// The document model is serialised to versioned JSON ("formatVersion"); image assets are stored
/// once each as separate container entries. Older format versions are migrated on load.
/// All functions are thread-safe with respect to their inputs, so saving runs off the GUI thread.
class ProjectSerializer
{
public:
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kFileSuffix = "classboard";

    static QByteArray serialize(const DocumentContents& contents);
    static bool deserialize(const QByteArray& bytes, DocumentContents* out, QString* error, int* skippedObjects = nullptr);

    /// Atomic save (write to temporary file, then rename): an interrupted save never corrupts the
    /// previous version of the file.
    static bool save(const QString& path, const DocumentContents& contents, QString* error);
    static bool load(const QString& path, DocumentContents* out, QString* error, int* skippedObjects = nullptr);

    /// Writes bytes atomically.
    static bool writeFileAtomically(const QString& path, const QByteArray& bytes, QString* error);

    /// Upgrades a parsed document JSON from an older formatVersion to the current one.
    static bool migrate(QJsonObject& root, QString* error);

    static QJsonObject documentToJson(const DocumentContents& contents);
    static bool documentFromJson(const QJsonObject& root, DocumentContents* out, int* skippedObjects);
};

} // namespace cb
