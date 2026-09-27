#include "SalesController.h"

#include <cmath>

#include "../domain/Attrs.h"

namespace
{
// Frontera QML: QML solo maneja double; el dominio usa Money (céntimos).
// Totales/montos double → Money::fromCop + validación fail-closed
// (finito, no negativo; >2 decimales por redondeo half-up en fromCop).
bool checkCopInput(double cop, QString &error)
{
    if (!std::isfinite(cop)) {
        error = QStringLiteral("Importe inválido");
        return false;
    }
    if (cop < 0.0) {
        error = QStringLiteral("El importe no puede ser negativo");
        return false;
    }
    return true;
}
} // namespace

SalesController::SalesController(SaleRepository *sales, SalesService *service,
                                 SerialRepository *serials, ProductRepository *products,
                                 QObject *parent)
    : QObject(parent), m_repos(sales), m_service(service), m_serials(serials), m_products(products),
      m_saleModel(this)
{
    refresh();
}

QVariantMap SalesController::toMap(const Sale &s)
{
    QVariantMap pay;
    for (auto it = s.payments.begin(); it != s.payments.end(); ++it)
        pay[it.key()] = it.value().toCop();
    return {{"id", s.id},
            {"date", s.date},
            {"client", s.client},
            {"total", s.total.toCop()},
            {"subtotal", s.subtotal.toCop()},
            {"tax", s.tax.toCop()},
            {"discount", s.discount.toCop()},
            {"status", s.status},
            {"docType", s.docType},
            {"payment", s.payment},
            {"payments", pay},
            {"paid", s.paid.toCop()},
            {"balance", s.balance.toCop()},
            {"cufe", s.dianCufe},
            {"parentId", s.parentId},
            {"reason", s.reason}};
}

void SalesController::refresh()
{
    m_pagedActive = false;
    m_modelActive = false;
    m_sales.clear();
    for (const Sale &s : m_repos->list())
        m_sales << toMap(s);
    m_totalCount = m_sales.size();
    emit salesChanged();
}

void SalesController::refreshPaged(int page, int pageSize)
{
    // Fase 4: página servidor; pageSize <= 0 equivale a refresh().
    m_totalCount = m_repos->count();
    if (pageSize <= 0) {
        refresh();
        return;
    }
    m_sales.clear();
    for (const Sale &s : m_repos->listPaged(pageSize, qMax(0, page) * pageSize))
        m_sales << toMap(s);
    emit salesChanged();
}

void SalesController::searchPaged(const QString &text, const QString &sortKey, bool sortAsc,
                                  int page, int pageSize)
{
    // Fase 4: filtro + orden + página en servidor.
    m_lastText = text;
    m_lastSortKey = sortKey;
    m_lastSortAsc = sortAsc;
    m_lastPage = page;
    m_lastSize = pageSize;
    m_pagedActive = true;
    m_modelActive = false;
    m_totalCount = m_repos->countSearch(text);
    if (pageSize <= 0)
        pageSize = m_totalCount;
    m_sales.clear();
    for (const Sale &s :
         m_repos->searchPaged(text, sortKey, sortAsc, pageSize, qMax(0, page) * pageSize))
        m_sales << toMap(s);
    emit salesChanged();
}

void SalesController::reloadSales()
{
    if (m_modelActive) {
        searchSales(m_modelText, m_modelSortKey, m_modelSortAsc);
        return;
    }
    if (m_pagedActive)
        searchPaged(m_lastText, m_lastSortKey, m_lastSortAsc, m_lastPage, m_lastSize);
    else
        refresh();
}

void SalesController::searchSales(const QString &text, const QString &sortKey, bool sortAsc)
{
    // Fase 4: reinicia el scroll infinito (página 0 al modelo).
    m_modelActive = true;
    m_modelSortKey = sortKey;
    m_modelSortAsc = sortAsc;
    m_modelPage = 0;
    m_totalCount = m_repos->countSearch(text);
    m_saleModel.setTotalCount(m_totalCount);
    QVariantList rows;
    for (const Sale &s : m_repos->searchPaged(text, sortKey, sortAsc, ModelPageSize, 0))
        rows << toMap(s);
    m_saleModel.setRows(rows);
    m_modelPage = 1;
    emit salesChanged();
}

void SalesController::fetchMoreSales()
{
    // Fase 4: anexa el siguiente lote si el servidor tiene más.
    if (!m_saleModel.canFetchMore())
        return;
    QVariantList rows;
    for (const Sale &s : m_repos->searchPaged(m_modelText, m_modelSortKey, m_modelSortAsc,
                                              ModelPageSize, m_modelPage * ModelPageSize))
        rows << toMap(s);
    if (rows.isEmpty()) {
        m_saleModel.setTotalCount(m_saleModel.rowCount());
        return;
    }
    m_saleModel.appendRows(rows);
    ++m_modelPage;
}

QVariantMap SalesController::detail(const QString &id) const
{
    const auto s = m_repos->find(id);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("No encontrada")}};
    QVariantMap d = toMap(*s);
    QVariantList items;
    for (const SaleItem &it : m_repos->itemsFor(id)) {
        QVariantMap m{{"productId", it.productId},
                      {"qty", it.qty},
                      {"subtotal", it.subtotal.toCop()},
                      {"serial", it.serial}};
        if (it.attrsJson.trimmed() != QStringLiteral("{}") && !it.attrsJson.trimmed().isEmpty())
            m["attrs"] = it.attrsJson;
        items << m;
    }
    d["items"] = items;
    d["ok"] = true;
    return d;
}

QVariantMap SalesController::advance(const QString &id, const QString &status, const QString &user)
{
    const auto r = m_repos->advanceStatus(id, status, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}};
}

QVariantMap SalesController::cancel(const QString &id, const QString &reason, const QString &user,
                                    const QString &role)
{
    const auto r = m_service->cancel(id, reason, user, role);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}};
}

QVariantMap SalesController::createDoc(const QString &type, const QString &client, double total,
                                       const QString &user)
{
    QString err;
    if (!checkCopInput(total, err))
        return {{"ok", false}, {"error", QStringLiteral("Documento: ") + err}};
    const auto r = m_repos->createDocument(type, client, Money::fromCop(total), user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::convert(const QString &originId, const QString &targetType,
                                      const QString &user)
{
    const auto r = m_repos->convertDocument(originId, targetType, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::creditNote(const QString &id, double amount, const QString &reason,
                                        const QString &user)
{
    QString err;
    if (!checkCopInput(amount, err))
        return {{"ok", false}, {"error", QStringLiteral("Nota crédito: ") + err}};
    const auto r = m_repos->createCreditNote(id, Money::fromCop(amount), reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::debitNote(const QString &id, double amount, const QString &reason,
                                       const QString &user)
{
    QString err;
    if (!checkCopInput(amount, err))
        return {{"ok", false}, {"error", QStringLiteral("Nota cargo: ") + err}};
    const auto r = m_repos->createDebitNote(id, Money::fromCop(amount), reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadSales();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::warrantyFor(const QString &serial) const
{
    if (!m_serials || !m_products)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de seriales")}};
    const auto s = m_serials->find(serial);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("Serial no registrado")}};
    int months = 12;
    if (const auto p = m_products->findBySku(s->sku))
        months = Attrs::integer(p->attrsJson, Attrs::KWarrantyMonths, 12);
    QVariantMap r = m_serials->warrantyStatus(serial, months);
    r["ok"] = r.value(QStringLiteral("error"), {}).toString().isEmpty();
    return r;
}

QVariantMap SalesController::markRma(const QString &serial, const QString &notes,
                                     const QString &user)
{
    Q_UNUSED(user);
    if (!m_serials)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de seriales")}};
    const auto r = m_serials->setStatus(serial, QStringLiteral("rma"), notes);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}};
}
