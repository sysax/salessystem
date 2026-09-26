#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/SaleRepository.h"
#include "../repositories/SerialRepository.h"
#include "../repositories/ProductRepository.h"
#include "../services/SalesService.h"
#include "PagedListModel.h"

// Historial, documentos y notas (antes SalesScreen).
class SalesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sales READ sales NOTIFY salesChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY salesChanged)
    Q_PROPERTY(PagedListModel *saleModel READ saleModel CONSTANT)

  public:
    explicit SalesController(SaleRepository *sales, SalesService *service,
                             SerialRepository *serials = nullptr,
                             ProductRepository *products = nullptr, QObject *parent = nullptr);

    QVariantList sales() const
    {
        return m_sales;
    }
    int totalCount() const
    {
        return m_totalCount;
    }
    PagedListModel *saleModel()
    {
        return &m_saleModel;
    }

    Q_INVOKABLE void refresh();
    // Fase 4: página servidor (page 0-based; pageSize <= 0 = todo).
    Q_INVOKABLE void refreshPaged(int page, int pageSize);
    // Fase 4: filtro texto + orden servidor + página (sustituye el
    // filtrado/ordenado en cliente de SalesPage).
    Q_INVOKABLE void searchPaged(const QString &text, const QString &sortKey, bool sortAsc,
                                 int page, int pageSize);
    // Fase 4: scroll infinito sobre el modelo incremental (página fija 30).
    Q_INVOKABLE void searchSales(const QString &text, const QString &sortKey, bool sortAsc);
    Q_INVOKABLE void fetchMoreSales();
    Q_INVOKABLE QVariantMap detail(const QString &id) const;
    Q_INVOKABLE QVariantMap advance(const QString &id, const QString &status, const QString &user);
    Q_INVOKABLE QVariantMap cancel(const QString &id, const QString &reason, const QString &user,
                                   const QString &role = {});
    Q_INVOKABLE QVariantMap createDoc(const QString &type, const QString &client, double total,
                                      const QString &user);
    Q_INVOKABLE QVariantMap creditNote(const QString &id, double amount, const QString &reason,
                                       const QString &user);
    Q_INVOKABLE QVariantMap debitNote(const QString &id, double amount, const QString &reason,
                                      const QString &user);
    // Fase 3: garantía por serial + RMA.
    Q_INVOKABLE QVariantMap warrantyFor(const QString &serial) const;
    Q_INVOKABLE QVariantMap markRma(const QString &serial, const QString &notes,
                                    const QString &user);

    static QVariantMap toMap(const Sale &s);

  signals:
    void salesChanged();

  private:
    // Fase 4: re-ejecuta la última consulta (paginada o no) tras mutar.
    void reloadSales();

    SaleRepository *m_repos = nullptr;
    SalesService *m_service = nullptr;
    SerialRepository *m_serials = nullptr;
    ProductRepository *m_products = nullptr;
    QVariantList m_sales;
    int m_totalCount = 0;
    // Última consulta (sticky para advance/cancel/docs).
    QString m_lastText;
    QString m_lastSortKey = QStringLiteral("id");
    bool m_lastSortAsc = false;
    int m_lastPage = 0;
    int m_lastSize = 0;
    bool m_pagedActive = false;
    // Fase 4: scroll infinito (tamaño de lote servidor).
    PagedListModel m_saleModel;
    QString m_modelText;
    QString m_modelSortKey = QStringLiteral("id");
    bool m_modelSortAsc = false;
    int m_modelPage = 0;
    bool m_modelActive = false;
    static constexpr int ModelPageSize = 30;
};
