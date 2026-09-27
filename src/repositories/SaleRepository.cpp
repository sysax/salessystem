#include "SaleRepository.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include "../core/Transaction.h"
#include "../services/Dian.h"
#include "../services/SettingsService.h"
#include "Counters.h"

const QStringList SaleRepository::EstadosVenta = {
    QStringLiteral("Cotización"), QStringLiteral("Pedido"),    QStringLiteral("Facturada"),
    QStringLiteral("Pagada"),     QStringLiteral("Entregada"), QStringLiteral("Cerrada"),
};
const QStringList SaleRepository::DocTypes = {
    QStringLiteral("Cotización"), QStringLiteral("Pedido"),       QStringLiteral("Remisión"),
    QStringLiteral("Factura"),    QStringLiteral("Nota crédito"), QStringLiteral("Nota cargo"),
};

SaleRepository::SaleRepository(QSqlDatabase db, ClientRepository *clients, CajaRepository *caja,
                               AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_clients(clients), m_caja(caja), m_audit(audit)
{
    m_locations = new LocationRepository(m_db, m_audit, this);
}

void SaleRepository::setSettings(SettingsService *s)
{
    m_settings = s;
}

namespace
{
// Fase 3: defaults históricos de series (misma fuente que
// SettingsService::defaultFolioSeries; aquí en forma prefix/counter para
// no duplicar literales dentro de createDocument).
void folioDefaults(QMap<QString, QString> &prefixes, QMap<QString, QString> &counters)
{
    const auto series = SettingsService::defaultFolioSeries();
    for (auto it = series.begin(); it != series.end(); ++it) {
        prefixes[it.key()] = it.value().prefix;
        counters[it.key()] = it.value().counter;
    }
}
} // namespace

Sale SaleRepository::rowToSale(const QSqlQuery &q)
{
    Sale s;
    s.id = q.value(QStringLiteral("id")).toString();
    s.date = q.value(QStringLiteral("date")).toString();
    s.client = q.value(QStringLiteral("client")).toString();
    s.vendedor = q.value(QStringLiteral("vendedor")).toString();
    s.total = Money::fromCop(q.value(QStringLiteral("total")).toDouble());
    s.subtotal = Money::fromCop(q.value(QStringLiteral("subtotal")).toDouble());
    s.tax = Money::fromCop(q.value(QStringLiteral("tax")).toDouble());
    s.discount = Money::fromCop(q.value(QStringLiteral("discount")).toDouble());
    s.promo = q.value(QStringLiteral("promo")).toString();
    s.status = q.value(QStringLiteral("status")).toString();
    s.docType = q.value(QStringLiteral("doc_type")).toString();
    s.payment = q.value(QStringLiteral("payment")).toString();
    const QByteArray raw = q.value(QStringLiteral("payments_json")).toByteArray();
    const QJsonObject obj = QJsonDocument::fromJson(raw.isEmpty() ? "{}" : raw).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        s.payments[it.key()] = Money::fromCop(it.value().toDouble());
    s.paid = Money::fromCop(q.value(QStringLiteral("paid")).toDouble());
    s.balance = Money::fromCop(q.value(QStringLiteral("balance")).toDouble());
    s.due = q.value(QStringLiteral("due")).toString();
    s.estado = q.value(QStringLiteral("estado")).toString();
    s.dianCufe = q.value(QStringLiteral("dian_cufe")).toString();
    s.dianStatus = q.value(QStringLiteral("dian_status")).toString();
    // Columna aditiva Fase 1: en BDs legadas aún no existe → "" (QVariant inválido).
    s.taxBreakdown = q.value(QStringLiteral("tax_breakdown")).toString();
    // Multitienda: columna aditiva; '' = mixta/legacy (visible en todos).
    s.businessType = q.value(QStringLiteral("business_type")).toString().trimmed();
    // Fase 5: trazabilidad documental (columnas aditivas; '' en legadas).
    s.parentId = q.value(QStringLiteral("parent_id")).toString();
    s.reason = q.value(QStringLiteral("reason")).toString();
    // Fase 6: almacén origen (columna aditiva; QVariant inválido en BDs
    // viejas → Principal).
    {
        const QVariant lv = q.value(QStringLiteral("location_id"));
        s.locationId = lv.isValid() && !lv.isNull() ? lv.toInt() : LocationRepository::kPrincipalId;
        if (s.locationId <= 0)
            s.locationId = LocationRepository::kPrincipalId;
    }
    return s;
}

QList<Sale> SaleRepository::list() const
{
    return listPaged(-1, 0);
}

QList<Sale> SaleRepository::listPaged(int limit, int offset) const
{
    // Fase 4: paginación servidor (limit < 0 = sin límite, compatible).
    QList<Sale> out;
    QSqlQuery q(m_db);
    const QString page = limit >= 0
                             ? QStringLiteral(" LIMIT %1 OFFSET %2").arg(limit).arg(qMax(0, offset))
                             : QString();
    if (!q.exec(QStringLiteral("SELECT * FROM sales ORDER BY date DESC, id DESC") + page))
        return out;
    while (q.next())
        out << rowToSale(q);
    return out;
}

int SaleRepository::count() const
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM sales")) || !q.next())
        return 0;
    return q.value(0).toInt();
}

QList<Sale> SaleRepository::searchPaged(const QString &text, const QString &sortKey, bool sortAsc,
                                        int limit, int offset) const
{
    // Fase 4: filtro texto (folio/cliente/estado/documento) + orden servidor
    // con whitelist + página. limit < 0 = sin límite.
    static const QSet<QString> kSort
        = {QStringLiteral("id"), QStringLiteral("date"), QStringLiteral("client"),
           QStringLiteral("total"), QStringLiteral("status")};
    const QString sk = kSort.contains(sortKey.trimmed().toLower()) ? sortKey.trimmed().toLower()
                                                                   : QStringLiteral("id");
    const QString order = QStringLiteral(" ORDER BY ") + sk
                          + (sortAsc ? QStringLiteral(" ASC") : QStringLiteral(" DESC"));
    const QString page = limit >= 0
                             ? QStringLiteral(" LIMIT %1 OFFSET %2").arg(limit).arg(qMax(0, offset))
                             : QString();
    const QString t = text.trimmed().toLower();
    QList<Sale> out;
    QSqlQuery q(m_db);
    if (t.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT * FROM sales") + order + page))
            return out;
    } else {
        q.prepare(
            QStringLiteral("SELECT * FROM sales WHERE (lower(id) LIKE ? OR lower(client) LIKE "
                           "? OR lower(status) LIKE ? OR lower(doc_type) LIKE ?)")
            + order + page);
        const QString like = u'%' + t + u'%';
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
        if (!q.exec())
            return out;
    }
    while (q.next())
        out << rowToSale(q);
    return out;
}

int SaleRepository::countSearch(const QString &text) const
{
    const QString t = text.trimmed().toLower();
    QSqlQuery q(m_db);
    if (t.isEmpty()) {
        if (q.exec(QStringLiteral("SELECT COUNT(*) FROM sales")) && q.next())
            return q.value(0).toInt();
        return 0;
    }
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM sales WHERE (lower(id) LIKE ? OR lower(client) "
                             "LIKE ? OR lower(status) LIKE ? OR lower(doc_type) LIKE ?)"));
    const QString like = u'%' + t + u'%';
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

std::optional<Sale> SaleRepository::find(const QString &id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM sales WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToSale(q);
    return std::nullopt;
}

QList<SaleItem> SaleRepository::itemsFor(const QString &saleId) const
{
    QList<SaleItem> out;
    QSqlQuery q(m_db);
    // SELECT *: las columnas Fase 3 (attrs_json, serial) pueden no existir en
    // BDs legadas — q.value() con nombre ausente retorna inválido (patrón
    // rowToSale/tax_breakdown), en vez de fallar como la lista explícita.
    q.prepare(QStringLiteral("SELECT * FROM sale_items WHERE sale_id=?"));
    q.addBindValue(saleId);
    if (!q.exec())
        return out;
    while (q.next()) {
        SaleItem it;
        it.productId = q.value(QStringLiteral("product_id")).toInt();
        it.qty = q.value(QStringLiteral("qty")).toDouble();
        it.subtotal = Money::fromCop(q.value(QStringLiteral("subtotal")).toDouble());
        it.attrsJson = q.value(QStringLiteral("attrs_json")).toString();
        if (it.attrsJson.trimmed().isEmpty())
            it.attrsJson = QStringLiteral("{}");
        it.serial = q.value(QStringLiteral("serial")).toString();
        out << it;
    }
    return out;
}

Result<Sale> SaleRepository::create(const NewSale &s)
{
    // Estado y descripción de pagos (réplica exacta de create_sale)
    QMap<QString, Money> payments;
    for (auto it = s.payments.begin(); it != s.payments.end(); ++it) {
        if (it.value().isPositive())
            payments[it.key()] = it.value();
    }
    QString status, paymentStr;
    if (!payments.isEmpty()) {
        const bool hasCredit
            = payments.value(QStringLiteral("credito"), Money::zero()).isPositive();
        status = hasCredit ? QStringLiteral("Pendiente") : QStringLiteral("Pagada");
        paymentStr = QStringList(payments.keys()).join(u'+');
    } else {
        payments.clear();
        status = s.paymentMethod == QLatin1String("Credito") ? QStringLiteral("Pendiente")
                                                             : QStringLiteral("Pagada");
        paymentStr = s.paymentMethod;
    }

    // Folio y CUFE se generan DENTRO de la transacción (ver abajo):
    // el contador es leer-luego-escribir y fuera del lock dos cajas
    // concurrentes obtendrían el mismo folio (o SQLITE_BUSY_SNAPSHOT
    // sin espera en autocommit). Dentro de BEGIN IMMEDIATE se serializa.
    Money paidVal, balanceVal;
    const Money creditPart = payments.value(QStringLiteral("credito"), Money::zero());
    if (status == QLatin1String("Pagada")) {
        paidVal = s.total;
        balanceVal = Money::zero();
    } else if (!payments.isEmpty()) {
        paidVal = s.total - creditPart;
        balanceVal = payments.contains(QStringLiteral("credito")) ? creditPart : s.total;
    } else {
        paidVal = Money::zero();
        balanceVal = s.total;
    }

    QString docType = s.docType;
    if (docType.isEmpty() || docType == QLatin1String("Factura electrónica DIAN"))
        docType = Dian::defaultDocType(m_db);
    const QString dianStatus
        = s.offline ? QStringLiteral("PENDIENTE_OFFLINE") : QStringLiteral("SINCRONIZADO");

    const QString today = QDate::currentDate().toString(Qt::ISODate);
    // Fase 3: días de crédito externalizados (0 = default 15).
    const int creditDays = s.creditDays > 0 ? s.creditDays : 15;
    const QString due = status == QLatin1String("Pendiente")
                            ? QDate::currentDate().addDays(creditDays).toString(Qt::ISODate)
                            : today;

    QMap<QString, Money> storedPayments
        = payments.isEmpty() ? QMap<QString, Money>{{paymentStr.toLower(), s.total}} : payments;
    QJsonObject payObj;
    for (auto it = storedPayments.begin(); it != storedPayments.end(); ++it)
        payObj[it.key()] = it.value().toCop();

    QSqlQuery q(m_db);
    // Fase 1: todo el flujo multi-escritura (folio + venta + líneas +
    // stock + crédito + caja) en una sola transacción. Cualquier fallo
    // revierte todo vía ~Transaction (sin ventas a medias ni stock
    // descontado sin venta). Anidable: si el llamador (SalesService) ya
    // abrió una transacción, esto es un SAVEPOINT.
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    const QString folio = Counters::next(m_db, QStringLiteral("SALE_COUNTER"), QStringLiteral("V"));
    const QString cufe = Dian::generateCufe(m_db, folio);
    // Fase 6: la venta descuenta el almacén indicado (default Principal).
    const int saleLoc = s.locationId > 0 ? s.locationId : LocationRepository::kPrincipalId;
    q.prepare(QStringLiteral(
        "INSERT INTO sales (id, date, client, vendedor, total, subtotal, tax, discount, promo, "
        "status, doc_type, payment, payments_json, paid, balance, due, estado, dian_cufe, "
        "dian_status, tax_breakdown, business_type, location_id) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(folio);
    q.addBindValue(today);
    q.addBindValue(s.clientName);
    q.addBindValue(s.vendedor);
    q.addBindValue(s.total.toCop());
    q.addBindValue((!s.subtotal.isZero() ? s.subtotal : s.total + s.discount - s.tax).toCop());
    q.addBindValue(s.tax.toCop());
    q.addBindValue(s.discount.toCop());
    q.addBindValue(s.promoCode.isEmpty() ? QVariant() : s.promoCode);
    q.addBindValue(status);
    q.addBindValue(docType);
    q.addBindValue(paymentStr);
    q.addBindValue(QString::fromUtf8(QJsonDocument(payObj).toJson(QJsonDocument::Compact)));
    q.addBindValue(paidVal.toCop());
    q.addBindValue(balanceVal.toCop());
    q.addBindValue(due);
    q.addBindValue(status);
    q.addBindValue(cufe);
    q.addBindValue(dianStatus);
    q.addBindValue(s.taxBreakdownJson);
    q.addBindValue(s.businessType.trimmed());
    q.addBindValue(saleLoc);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());

    for (const SaleItem &it : s.items) {
        // Fase 1: decremento atómico (sin TOCTOU leer-luego-escribir).
        // 0 filas => otro hilo/caja vendió primero o no hay stock.
        // Fase 6: además del agregado, se descuenta el ledger del almacén
        // de la venta (misma tx): la venta consume sus existencias.
        QSqlQuery skuQ(m_db);
        skuQ.prepare(QStringLiteral("SELECT sku FROM products WHERE id=?"));
        skuQ.addBindValue(it.productId);
        if (!skuQ.exec() || !skuQ.next())
            return Result<Sale>::failure(
                QStringLiteral("Producto ID %1 no existe").arg(it.productId));
        const QString sku = skuQ.value(0).toString();
        if (!m_locations->takeLedger(sku, saleLoc, it.qty))
            return Result<Sale>::failure(
                QStringLiteral("Stock insuficiente (producto ID %1)").arg(it.productId));
        QSqlQuery up(m_db);
        up.prepare(
            QStringLiteral("UPDATE products SET stock = stock - ? WHERE id = ? AND stock >= ?"));
        up.addBindValue(it.qty);
        up.addBindValue(it.productId);
        up.addBindValue(it.qty);
        if (!up.exec())
            return Result<Sale>::failure(QStringLiteral("Stock: ") + up.lastError().text());
        if (up.numRowsAffected() != 1)
            return Result<Sale>::failure(
                QStringLiteral("Stock insuficiente (producto ID %1)").arg(it.productId));
        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal, attrs_json, serial) "
            "VALUES (?,?,?,?,?,?)"));
        ins.addBindValue(folio);
        ins.addBindValue(it.productId);
        ins.addBindValue(it.qty);
        ins.addBindValue(it.subtotal.toCop());
        ins.addBindValue(it.attrsJson.trimmed().isEmpty() ? QStringLiteral("{}") : it.attrsJson);
        ins.addBindValue(it.serial);
        if (!ins.exec())
            return Result<Sale>::failure(QStringLiteral("Línea de venta: ")
                                         + ins.lastError().text());
    }

    // Crédito a la cuenta del cliente
    const Money creditAmount = status == QLatin1String("Pendiente")
                                   ? (payments.isEmpty() ? s.total : creditPart)
                                   : Money::zero();
    if (creditAmount.isPositive() && m_clients
        && !m_clients->addCredit(s.clientName, creditAmount))
        return Result<Sale>::failure(QStringLiteral("No se pudo cargar el crédito al cliente"));

    if (m_caja && !m_caja->recordSale(folio, s.total))
        return Result<Sale>::failure(QStringLiteral("No se pudo registrar la venta en caja"));

    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar la venta"));
    const auto done = find(folio);
    if (!done)
        return Result<Sale>::failure(
            QStringLiteral("Venta %1 no encontrada tras crear").arg(folio));
    return Result<Sale>::success(*done);
}

Result<Sale> SaleRepository::createDocument(const QString &docType, const QString &client,
                                             Money total, const QString &user,
                                             const QString &parentId, const QString &reason)
{
    // Fase 5: cada tipo documental con folio y contador propios. Antes,
    // Remisión/Factura/Nota cargo compartían SALE_COUNTER con las ventas
    // POS (un V001 podía ser remisión, factura, ticket o nota de cargo).
    // Fase 3: series externalizadas en settings (sin settings = defaults).
    QMap<QString, QString> prefixes;
    QMap<QString, QString> counters;
    folioDefaults(prefixes, counters);
    if (m_settings) {
        const auto series = m_settings->folioSeries();
        for (auto it = series.begin(); it != series.end(); ++it) {
            if (prefixes.contains(it.key()) && !it.value().prefix.isEmpty()
                && !it.value().counter.isEmpty()) {
                prefixes[it.key()] = it.value().prefix;
                counters[it.key()] = it.value().counter;
            }
        }
    }
    if (!prefixes.contains(docType))
        return Result<Sale>::failure(
            QStringLiteral("doc_type debe ser %1").arg(DocTypes.join(u", ")));
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    // Fase 1: documento + auditoría en una transacción (el folio vive
    // dentro: sin duplicados entre cajas concurrentes, sin huecos si falla).
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    const QString folio = Counters::next(m_db, counters[docType], prefixes[docType]);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sales (id, date, client, vendedor, total, subtotal, tax, discount, status, "
        "doc_type, payment, payments_json, paid, balance, estado, parent_id, reason, location_id) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(folio);
    q.addBindValue(today);
    q.addBindValue(client);
    q.addBindValue(user);
    q.addBindValue(total.toCop());
    q.addBindValue(total.toCop());
    q.addBindValue(0.0);
    q.addBindValue(0.0);
    q.addBindValue(docType);
    q.addBindValue(docType);
    q.addBindValue(QString());
    q.addBindValue(QStringLiteral("{}"));
    q.addBindValue(0.0);
    q.addBindValue(total.toCop());
    q.addBindValue(docType);
    q.addBindValue(parentId);
    q.addBindValue(reason.trimmed().left(280));
    q.addBindValue(LocationRepository::kPrincipalId);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());
    if (m_audit)
        m_audit->log(user,
                     QStringLiteral("doc_%1_creado").arg(docType.toLower().replace(u' ', u'_')),
                     QStringLiteral("%1 %2 $%3").arg(folio, client).arg(total.toCop(), 0, 'f', 0));
    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar el documento"));
    const auto created = find(folio);
    if (!created)
        return Result<Sale>::failure(
            QStringLiteral("Documento %1 no encontrado tras crear").arg(folio));
    return Result<Sale>::success(*created);
}

bool SaleRepository::transitionAllowed(const QString &from, const QString &to)
{
    // Fase 5: máquina de estados documental. Ventas POS (Pagada/Pendiente)
    // y Cancelada se gestionan por sus propios caminos (create/cancel).
    static const QMap<QString, QStringList> kAllowed = {
        {QStringLiteral("Cotización"), {QStringLiteral("Pedido")}},
        {QStringLiteral("Pedido"), {QStringLiteral("Facturada")}},
        {QStringLiteral("Facturada"),
         {QStringLiteral("Pagada"), QStringLiteral("Entregada")}},
        {QStringLiteral("Pagada"),
         {QStringLiteral("Entregada"), QStringLiteral("Cerrada")}},
        {QStringLiteral("Entregada"), {QStringLiteral("Cerrada")}},
        // Remisión acompaña entrega sin cambiar el estado de facturación.
        {QStringLiteral("Remisión"), {QStringLiteral("Entregada")}},
    };
    return kAllowed.value(from).contains(to);
}

Result<Sale> SaleRepository::convertDocument(const QString &originFolio,
                                             const QString &targetDocType, const QString &user)
{
    const auto origin = find(originFolio);
    if (!origin)
        return Result<Sale>::failure(QStringLiteral("Documento %1 no encontrado").arg(originFolio));
    if (!DocTypes.contains(targetDocType))
        return Result<Sale>::failure(
            QStringLiteral("doc_type debe ser %1").arg(DocTypes.join(u", ")));
    // Conversión válida: el estado del origen debe poder avanzar al estado
    // que representa el destino (Cotización→Pedido→Facturada).
    static const QMap<QString, QString> kTargetState = {
        {QStringLiteral("Pedido"), QStringLiteral("Pedido")},
        {QStringLiteral("Factura"), QStringLiteral("Facturada")},
        {QStringLiteral("Remisión"), QStringLiteral("Entregada")},
    };
    const QString wantState = kTargetState.value(
        targetDocType, targetDocType == QStringLiteral("Cotización")
                           ? QStringLiteral("Cotización")
                           : QString());
    if (wantState.isEmpty() || !transitionAllowed(origin->status, wantState))
        return Result<Sale>::failure(QStringLiteral("No se puede convertir %1 (%2) a %3")
                                         .arg(originFolio, origin->status, targetDocType));
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    auto created = createDocument(targetDocType, origin->client, origin->total, user, originFolio,
                                  QStringLiteral("Conversión de %1").arg(originFolio));
    if (!created.ok())
        return created;
    // Clonar líneas al nuevo folio (la cotización/pedido ya trae detalle).
    for (const SaleItem &it : itemsFor(originFolio)) {
        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal, attrs_json, serial) "
            "VALUES (?,?,?,?,?,?)"));
        ins.addBindValue(created.value().id);
        ins.addBindValue(it.productId);
        ins.addBindValue(it.qty);
        ins.addBindValue(it.subtotal.toCop());
        ins.addBindValue(it.attrsJson.trimmed().isEmpty() ? QStringLiteral("{}") : it.attrsJson);
        ins.addBindValue(it.serial);
        if (!ins.exec())
            return Result<Sale>::failure(QStringLiteral("Línea de documento: ")
                                         + ins.lastError().text());
    }
    // El origen queda Cerrado como consumido (trazable vía parent_id).
    QSqlQuery close(m_db);
    close.prepare(QStringLiteral("UPDATE sales SET status='Cerrada', estado='Cerrada' WHERE id=?"));
    close.addBindValue(originFolio);
    if (!close.exec())
        return Result<Sale>::failure(close.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("doc_convertido"),
                     QStringLiteral("%1 -> %2").arg(originFolio, created.value().id));
    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar la conversión"));
    return Result<Sale>::success(*find(created.value().id));
}

Result<Sale> SaleRepository::markCancelled(const QString &saleId, const QString &reason,
                                           const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (s->status == QLatin1String("Cancelada"))
        return Result<Sale>::failure(QStringLiteral("Venta %1 ya está cancelada").arg(saleId));
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE sales SET status='Cancelada', estado='Cancelada', balance=0, reason=? WHERE id=?"));
    q.addBindValue(reason.trimmed().left(280));
    q.addBindValue(saleId);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("venta_cancelada"),
                     saleId + (reason.trimmed().isEmpty()
                                   ? QString()
                                   : QStringLiteral(" motivo: ") + reason.trimmed().left(120)));
    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar el cambio"));
    const auto cancelled = find(saleId);
    if (!cancelled)
        return Result<Sale>::failure(
            QStringLiteral("Venta %1 no encontrada tras actualizar").arg(saleId));
    return Result<Sale>::success(*cancelled);
}

Money SaleRepository::creditNotesTotal(const QString &parentId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(total),0) FROM sales WHERE parent_id=? AND "
                             "doc_type='Nota crédito' AND status!='Cancelada'"));
    q.addBindValue(parentId);
    if (!q.exec() || !q.next())
        return Money::zero();
    return Money::fromCop(q.value(0).toDouble());
}

Result<Sale> SaleRepository::advanceStatus(const QString &saleId, const QString &newStatus,
                                           const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (s->status == QLatin1String("Cancelada"))
        return Result<Sale>::failure(QStringLiteral("Venta cancelada no avanza"));
    // Fase 5: Cancelada solo vía cancel() con reversión (SalesService).
    // El camino directo dejaba stock/caja/crédito sin revertir.
    if (newStatus == QLatin1String("Cancelada"))
        return Result<Sale>::failure(
            QStringLiteral("Use anular con motivo (revierte stock, caja y crédito)"));
    // Fase 1: cambio de estado + auditoría en una transacción.
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    QSqlQuery q(m_db);
    if (!EstadosVenta.contains(newStatus))
        return Result<Sale>::failure(
            QStringLiteral("Estado debe ser %1")
                .arg((EstadosVenta + QStringList{QStringLiteral("Cancelada")}).join(u", ")));
    // Fase 5: respetar la máquina documental (sin saltos ni retrocesos).
    // Ventas POS (Pagada/Pendiente) avanzan a entrega/cierre; documentos
    // siguen Cotización→Pedido→Facturada→Pagada→Entregada→Cerrada.
    if (s->status != newStatus && EstadosVenta.contains(s->status)
        && !transitionAllowed(s->status, newStatus) && s->status != QStringLiteral("Remisión")
        && !(s->status == QLatin1String("Pendiente") && newStatus == QLatin1String("Pagada"))
        && !(s->status == QLatin1String("Pagada")
             && (newStatus == QLatin1String("Entregada") || newStatus == QLatin1String("Cerrada"))))
        return Result<Sale>::failure(QStringLiteral("Transición %1 → %2 no permitida")
                                         .arg(s->status, newStatus));
    if (newStatus == QLatin1String("Pagada") || newStatus == QLatin1String("Entregada")
        || newStatus == QLatin1String("Cerrada")) {
        q.prepare(QStringLiteral(
            "UPDATE sales SET status=?, estado=?, balance=0, paid=total WHERE id=?"));
    } else {
        q.prepare(QStringLiteral("UPDATE sales SET status=?, estado=? WHERE id=?"));
    }
    q.addBindValue(newStatus);
    q.addBindValue(newStatus);
    q.addBindValue(saleId);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("venta_avance"),
                     QStringLiteral("%1 -> %2").arg(saleId, newStatus));
    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar el cambio"));
    const auto advanced = find(saleId);
    if (!advanced)
        return Result<Sale>::failure(
            QStringLiteral("Venta %1 no encontrada tras actualizar").arg(saleId));
    return Result<Sale>::success(*advanced);
}

Result<Sale> SaleRepository::createCreditNote(const QString &saleId, Money amount,
                                              const QString &reason, const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    // Fase 5: la NC vive ligada a su factura (no a cotizaciones/pedidos) y
    // el acumulado de NCs nunca supera el total facturado.
    if (s->docType == QStringLiteral("Cotización") || s->docType == QLatin1String("Pedido")
        || s->status == QStringLiteral("Cotización") || s->status == QLatin1String("Pedido"))
        return Result<Sale>::failure(
            QStringLiteral("La nota crédito aplica sobre factura, no sobre %1").arg(s->status));
    if (!amount.isPositive() || amount > s->total)
        return Result<Sale>::failure(QStringLiteral("Monto inválido"));
    if (creditNotesTotal(saleId) + amount > s->total)
        return Result<Sale>::failure(QStringLiteral("Notas crédito acumulan $%1: supera el total $%2")
                                         .arg((creditNotesTotal(saleId) + amount).toCop(), 0, 'f', 0)
                                         .arg(s->total.toCop(), 0, 'f', 0));
    if (reason.trimmed().isEmpty())
        return Result<Sale>::failure(QStringLiteral("Motivo requerido"));
    auto note = createDocument(QStringLiteral("Nota crédito"), s->client, amount, user, saleId,
                               reason.trimmed());
    if (!note.ok())
        return note;
    if (m_audit)
        m_audit->log(user, QStringLiteral("nota_credito"),
                     QStringLiteral("%1 ref %2 $%3 %4")
                         .arg(note.value().id, saleId)
                         .arg(amount.toCop(), 0, 'f', 0)
                         .arg(reason.trimmed().left(20)));
    return note;
}

Result<Sale> SaleRepository::createDebitNote(const QString &saleId, Money amount,
                                             const QString &reason, const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (s->docType == QStringLiteral("Cotización") || s->docType == QLatin1String("Pedido"))
        return Result<Sale>::failure(
            QStringLiteral("La nota cargo aplica sobre factura, no sobre %1").arg(s->status));
    if (!amount.isPositive())
        return Result<Sale>::failure(QStringLiteral("Monto >0"));
    if (reason.trimmed().isEmpty())
        return Result<Sale>::failure(QStringLiteral("Motivo requerido"));
    auto note = createDocument(QStringLiteral("Nota cargo"), s->client, amount, user, saleId,
                               reason.trimmed());
    if (!note.ok())
        return note;
    if (m_audit)
        m_audit->log(
            user, QStringLiteral("nota_cargo"),
            QStringLiteral("%1 ref %2 $%3").arg(note.value().id, saleId).arg(amount.toCop(), 0, 'f', 0));
    return note;
}
