#pragma once

#include "core/Id.h"

#include <QObject>
#include <QVector>

namespace cb {

/// The set of selected objects on the current page (ordered by selection time).
class SelectionModel : public QObject
{
    Q_OBJECT
public:
    explicit SelectionModel(QObject* parent = nullptr);

    const QVector<ObjectId>& ids() const { return m_ids; }
    bool isEmpty() const { return m_ids.isEmpty(); }
    int count() const { return m_ids.size(); }
    bool contains(const ObjectId& id) const { return m_ids.contains(id); }

    void set(const QVector<ObjectId>& ids);
    void setSingle(const ObjectId& id);
    void add(const ObjectId& id);
    void toggle(const ObjectId& id);
    void remove(const ObjectId& id);
    void clear();

signals:
    void changed();

private:
    QVector<ObjectId> m_ids;
};

} // namespace cb
