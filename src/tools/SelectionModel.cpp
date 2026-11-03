#include "tools/SelectionModel.h"

namespace cb {

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
{
}

void SelectionModel::set(const QVector<ObjectId>& ids)
{
    if (ids == m_ids)
        return;
    m_ids = ids;
    emit changed();
}

void SelectionModel::setSingle(const ObjectId& id)
{
    set({id});
}

void SelectionModel::add(const ObjectId& id)
{
    if (m_ids.contains(id))
        return;
    m_ids.push_back(id);
    emit changed();
}

void SelectionModel::toggle(const ObjectId& id)
{
    if (m_ids.contains(id))
        remove(id);
    else
        add(id);
}

void SelectionModel::remove(const ObjectId& id)
{
    if (m_ids.removeAll(id) > 0)
        emit changed();
}

void SelectionModel::clear()
{
    if (m_ids.isEmpty())
        return;
    m_ids.clear();
    emit changed();
}

} // namespace cb
