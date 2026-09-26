#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

// Fase 4: modelo incremental genérico para listados grandes. Los roles
// nacen de las claves del primer lote (mapas homogéneos de los
// controladores: toMap). fetchMore() lo invoca la UI (scroll infinito);
// el controlador decide la página (servidor) y anexa con appendRows().
class PagedListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount WRITE setTotalCount NOTIFY totalCountChanged)
    Q_PROPERTY(bool canFetchMore READ canFetchMore NOTIFY countChanged)

  public:
    explicit PagedListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalCount() const
    {
        return m_totalCount;
    }
    void setTotalCount(int total);

    // true si hay más filas en el servidor que las cargadas.
    bool canFetchMore() const
    {
        return m_rows.size() < m_totalCount;
    }

    Q_INVOKABLE QVariantMap get(int row) const;

  signals:
    void totalCountChanged();
    void countChanged();

  public slots:
    void setRows(const QVariantList &rows);
    void appendRows(const QVariantList &rows);
    void clear();

  private:
    void rebuildRoles();

    QVariantList m_rows;
    QHash<int, QByteArray> m_roles;
    QStringList m_roleKeys;
    int m_totalCount = 0;
};
