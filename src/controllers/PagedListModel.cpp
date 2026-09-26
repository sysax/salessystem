#include "PagedListModel.h"

PagedListModel::PagedListModel(QObject *parent) : QAbstractListModel(parent)
{
}

int PagedListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant PagedListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const QVariantMap row = m_rows.at(index.row()).toMap();
    const int keyIdx = role - Qt::UserRole - 1;
    if (keyIdx < 0 || keyIdx >= m_roleKeys.size())
        return {};
    return row.value(m_roleKeys.at(keyIdx));
}

QHash<int, QByteArray> PagedListModel::roleNames() const
{
    return m_roles;
}

void PagedListModel::setTotalCount(int total)
{
    total = qMax(0, total);
    if (m_totalCount == total)
        return;
    m_totalCount = total;
    emit totalCountChanged();
    emit countChanged(); // canFetchMore pudo cambiar
}

QVariantMap PagedListModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    return m_rows.at(row).toMap();
}

void PagedListModel::rebuildRoles()
{
    m_roles.clear();
    m_roleKeys.clear();
    if (m_rows.isEmpty())
        return;
    const QVariantMap first = m_rows.first().toMap();
    int role = Qt::UserRole + 1;
    for (auto it = first.begin(); it != first.end(); ++it) {
        m_roleKeys << it.key();
        m_roles[role++] = it.key().toUtf8();
    }
}

void PagedListModel::setRows(const QVariantList &rows)
{
    beginResetModel();
    m_rows = rows;
    rebuildRoles();
    endResetModel();
    emit countChanged();
}

void PagedListModel::appendRows(const QVariantList &rows)
{
    if (rows.isEmpty())
        return;
    const bool firstBatch = m_rows.isEmpty();
    if (firstBatch) {
        setRows(rows);
        return;
    }
    const int first = m_rows.size();
    beginInsertRows(QModelIndex(), first, first + rows.size() - 1);
    m_rows.append(rows);
    endInsertRows();
    emit countChanged();
}

void PagedListModel::clear()
{
    if (m_rows.isEmpty() && m_totalCount == 0)
        return;
    beginResetModel();
    m_rows.clear();
    m_roles.clear();
    m_roleKeys.clear();
    m_totalCount = 0;
    endResetModel();
    emit totalCountChanged();
    emit countChanged();
}
