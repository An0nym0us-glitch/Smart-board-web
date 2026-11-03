#pragma once

#include <QString>
#include <QUuid>

namespace cb {

using ObjectId = QUuid;
using PageId = QUuid;

inline QUuid newId() { return QUuid::createUuid(); }
inline QString idToString(const QUuid& id) { return id.toString(QUuid::WithoutBraces); }
inline QUuid idFromString(const QString& s) { return QUuid::fromString(s); }

} // namespace cb
