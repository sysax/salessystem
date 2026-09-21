#include "SaleRepository.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include "../services/Dian.h"
#include "Counters.h"

const QStringList SaleRepository::EstadosVenta = {
    QStringLiteral("Cotización"), QStringLiteral("Pedido"), QStringLiteral("Facturada"),
    QStringLiteral("Pagada"), QStringLiteral("Entregada"), QStringLiteral("Cerrada"),
};
const QStringList SaleRepository::DocTypes = {
    QStringLiteral("Cotización"), QStringLiteral("Pedido"), QStringLiteral("Remisión"),
    QStringLiteral("Factura"), QStringLiteral("Nota crédito"), QStringLiteral("Nota cargo"),
};

SaleRepository::SaleRepository(QSqlDatabase db, ClientRepository *clients, CajaRepository *caja,
                               AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_clients(clients), m_caja(caja), m_audit(audit)
{
}

Sale SaleRepository::rowToSale(const QSqlQuery &q)
{
    Sale s;
    s.id = q.value(QStringLiteral("id")).toString();
    s.date = q.value(QStringLiteral("date")).toString();
    s.client = q.value(QStringLiteral("client")).toString();
    s.vendedor = q.value(QStringLiteral("vendedor")).toString();
    s.total = q.value(QStringLiteral("total")).toDouble();
    s.subtotal = q.value(QStringLiteral("subtotal")).toDouble();
    s.tax = q.value(QStringLiteral("tax")).toDouble();
    s.discount = q.value(QStringLiteral("discount")).toDouble();
    s.promo = q.value(QStringLiteral("promo")).toString();
    s.status = q.value(QStringLiteral("status")).toString();
    s.docType = q.value(QStringLiteral("doc_type")).toString();
    s.payment = q.value(QStringLiteral("payment")).toString();
    const QByteArray raw = q.value(QStringLiteral("payments_json")).toByteArray();
    const QJsonObject obj = QJsonDocument::fromJson(raw.isEmpty() ? "{}" : raw).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        s.payments[it.key()] = it.value().toDouble();
    s.paid = q.value(QStringLiteral("paid")).toDouble();
    s.balance = q.value(QStringLiteral("balance")).toDouble();
    s.due = q.value(QStringLiteral("due")).toString();
    s.estado = q.value(QStringLiteral("estado")).toString();
    s.dianCufe = q.value(QStringLiteral("dian_cufe")).toString();
    s.dianStatus = q.value(QStringLiteral("dian_status")).toString();
    return s;
}

QList<Sale> SaleRepository::list() const
{
    QList<Sale> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM sales ORDER BY date DESC, id DESC")))
        return out;
    while (q.next())
        out << rowToSale(q);
    return out;
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
    q.prepare(QStringLiteral("SELECT product_id, qty, subtotal FROM sale_items WHERE sale_id=?"));
    q.addBindValue(saleId);
    if (!q.exec())
        return out;
    while (q.next()) {
        SaleItem it;
        it.productId = q.value(0).toInt();
        it.qty = q.value(1).toInt();
        it.subtotal = q.value(2).toDouble();
        out << it;
    }
    return out;
}

Result<Sale> SaleRepository::create(const NewSale &s)
{
    // Estado y descripción de pagos (réplica exacta de create_sale)
    QMap<QString, double> payments;
    for (auto it = s.payments.begin(); it != s.payments.end(); ++it) {
        if (it.value() > 0)
            payments[it.key()] = it.value();
    }
    QString status, paymentStr;
    if (!payments.isEmpty()) {
        const bool hasCredit = payments.value(QStringLiteral("credito"), 0.0) > 0;
        status = hasCredit ? QStringLiteral("Pendiente") : QStringLiteral("Pagada");
        paymentStr = QStringList(payments.keys()).join(u'+');
    } else {
        payments.clear();
        status = s.paymentMethod == QLatin1String("Credito") ? QStringLiteral("Pendiente")
                                                             : QStringLiteral("Pagada");
        paymentStr = s.paymentMethod;
    }

    const QString folio = Counters::next(m_db, QStringLiteral("SALE_COUNTER"),
                                         QStringLiteral("V"));
    double paidVal, balanceVal;
    const double creditPart = payments.value(QStringLiteral("credito"), 0.0);
    if (status == QLatin1String("Pagada")) {
        paidVal = s.total;
        balanceVal = 0.0;
    } else if (!payments.isEmpty()) {
        paidVal = s.total - creditPart;
        balanceVal = payments.contains(QStringLiteral("credito")) ? creditPart : s.total;
    } else {
        paidVal = 0.0;
        balanceVal = s.total;
    }

    QString docType = s.docType;
    if (docType.isEmpty() || docType == QLatin1String("Factura electrónica DIAN"))
        docType = Dian::defaultDocType(m_db);
    const QString cufe = Dian::generateCufe(m_db, folio);
    const QString dianStatus =
        s.offline ? QStringLiteral("PENDIENTE_OFFLINE") : QStringLiteral("SINCRONIZADO");

    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const QString due = status == QLatin1String("Pendiente")
        ? QDate::currentDate().addDays(15).toString(Qt::ISODate)
        : today;

    QMap<QString, double> storedPayments = payments.isEmpty()
        ? QMap<QString, double>{{paymentStr.toLower(), s.total}}
        : payments;
    QJsonObject payObj;
    for (auto it = storedPayments.begin(); it != storedPayments.end(); ++it)
        payObj[it.key()] = it.value();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sales (id, date, client, vendedor, total, subtotal, tax, discount, promo, "
        "status, doc_type, payment, payments_json, paid, balance, due, estado, dian_cufe, "
        "dian_status) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(folio);
    q.addBindValue(today);
    q.addBindValue(s.clientName);
    q.addBindValue(s.vendedor);
    q.addBindValue(s.total);
    q.addBindValue(s.subtotal != 0 ? s.subtotal : s.total + s.discount - s.tax);
    q.addBindValue(s.tax);
    q.addBindValue(s.discount);
    q.addBindValue(s.promoCode.isEmpty() ? QVariant() : s.promoCode);
    q.addBindValue(status);
    q.addBindValue(docType);
    q.addBindValue(paymentStr);
    q.addBindValue(QString::fromUtf8(QJsonDocument(payObj).toJson(QJsonDocument::Compact)));
    q.addBindValue(paidVal);
    q.addBindValue(balanceVal);
    q.addBindValue(due);
    q.addBindValue(status);
    q.addBindValue(cufe);
    q.addBindValue(dianStatus);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());

    for (const SaleItem &it : s.items) {
        QSqlQuery cur(m_db);
        cur.prepare(QStringLiteral("SELECT stock FROM products WHERE id=?"));
        cur.addBindValue(it.productId);
        if (cur.exec() && cur.next()) {
            QSqlQuery up(m_db);
            up.prepare(QStringLiteral("UPDATE products SET stock=? WHERE id=?"));
            up.addBindValue(cur.value(0).toInt() - it.qty);
            up.addBindValue(it.productId);
            up.exec();
        }
        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal) VALUES (?,?,?,?)"));
        ins.addBindValue(folio);
        ins.addBindValue(it.productId);
        ins.addBindValue(it.qty);
        ins.addBindValue(it.subtotal);
        ins.exec();
    }

    // Crédito a la cuenta del cliente
    const double creditAmount = status == QLatin1String("Pendiente")
        ? (payments.isEmpty() ? s.total : creditPart)
        : 0.0;
    if (creditAmount > 0 && m_clients)
        m_clients->addCredit(s.clientName, creditAmount);

    if (m_caja)
        m_caja->recordSale(folio, s.total);

    return Result<Sale>::success(*find(folio));
}

Result<Sale> SaleRepository::createDocument(const QString &docType, const QString &client,
                                            double total, const QString &user)
{
    static const QMap<QString, QString> prefixes = {
        {QStringLiteral("Cotización"), QStringLiteral("COT")},
        {QStringLiteral("Pedido"), QStringLiteral("PED")},
        {QStringLiteral("Remisión"), QStringLiteral("REM")},
        {QStringLiteral("Factura"), QStringLiteral("V")},
        {QStringLiteral("Nota crédito"), QStringLiteral("NC")},
        {QStringLiteral("Nota cargo"), QStringLiteral("NCC")},
    };
    static const QMap<QString, QString> counters = {
        {QStringLiteral("Cotización"), QStringLiteral("QUOTE_COUNTER")},
        {QStringLiteral("Pedido"), QStringLiteral("ORDER_COUNTER")},
        {QStringLiteral("Remisión"), QStringLiteral("SALE_COUNTER")},
        {QStringLiteral("Factura"), QStringLiteral("SALE_COUNTER")},
        {QStringLiteral("Nota crédito"), QStringLiteral("CREDIT_NOTE_COUNTER")},
        {QStringLiteral("Nota cargo"), QStringLiteral("SALE_COUNTER")},
    };
    if (!prefixes.contains(docType))
        return Result<Sale>::failure(
            QStringLiteral("doc_type debe ser %1").arg(DocTypes.join(u", ")));
    const QString folio = Counters::next(m_db, counters[docType], prefixes[docType]);
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sales (id, date, client, vendedor, total, subtotal, tax, discount, status, "
        "doc_type, payment, payments_json, paid, balance, estado) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(folio);
    q.addBindValue(today);
    q.addBindValue(client);
    q.addBindValue(user);
    q.addBindValue(total);
    q.addBindValue(total);
    q.addBindValue(0.0);
    q.addBindValue(0.0);
    q.addBindValue(docType);
    q.addBindValue(docType);
    q.addBindValue(QString());
    q.addBindValue(QStringLiteral("{}"));
    q.addBindValue(0.0);
    q.addBindValue(total);
    q.addBindValue(docType);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("doc_%1_creado").arg(docType.toLower().replace(u' ', u'_')),
                     QStringLiteral("%1 %2 $%3").arg(folio, client).arg(total, 0, 'f', 0));
    return Result<Sale>::success(*find(folio));
}

Result<Sale> SaleRepository::advanceStatus(const QString &saleId, const QString &newStatus,
                                           const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(
            QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (s->status == QLatin1String("Cancelada"))
        return Result<Sale>::failure(QStringLiteral("Venta cancelada no avanza"));
    QSqlQuery q(m_db);
    if (newStatus == QLatin1String("Cancelada")) {
        q.prepare(QStringLiteral(
            "UPDATE sales SET status='Cancelada', estado='Cancelada', balance=0 WHERE id=?"));
        q.addBindValue(saleId);
        if (!q.exec())
            return Result<Sale>::failure(q.lastError().text());
        if (m_audit)
            m_audit->log(user, QStringLiteral("venta_cancelada"), saleId);
        return Result<Sale>::success(*find(saleId));
    }
    if (!EstadosVenta.contains(newStatus))
        return Result<Sale>::failure(
            QStringLiteral("Estado debe ser %1")
                .arg((EstadosVenta + QStringList{QStringLiteral("Cancelada")}).join(u", ")));
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
    return Result<Sale>::success(*find(saleId));
}

Result<Sale> SaleRepository::createCreditNote(const QString &saleId, double amount,
                                              const QString &reason, const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (amount <= 0 || amount > s->total)
        return Result<Sale>::failure(QStringLiteral("Monto inválido"));
    if (reason.trimmed().isEmpty())
        return Result<Sale>::failure(QStringLiteral("Motivo requerido"));
    auto note = createDocument(QStringLiteral("Nota crédito"), s->client, amount, user);
    if (!note.ok())
        return note;
    // NOTA: el Python original hacía UPDATE sales SET reason/ref (columnas
    // inexistentes → OperationalError). Aquí el motivo queda en bitácora.
    if (m_audit)
        m_audit->log(user, QStringLiteral("nota_credito"),
                     QStringLiteral("%1 ref %2 $%3 %4")
                         .arg(note.value().id, saleId)
                         .arg(amount, 0, 'f', 0)
                         .arg(reason.trimmed().left(20)));
    return note;
}

Result<Sale> SaleRepository::createDebitNote(const QString &saleId, double amount,
                                             const QString &reason, const QString &user)
{
    const auto s = find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (amount <= 0)
        return Result<Sale>::failure(QStringLiteral("Monto >0"));
    if (reason.trimmed().isEmpty())
        return Result<Sale>::failure(QStringLiteral("Motivo requerido"));
    auto note = createDocument(QStringLiteral("Nota cargo"), s->client, amount, user);
    if (!note.ok())
        return note;
    if (m_audit)
        m_audit->log(user, QStringLiteral("nota_cargo"),
                     QStringLiteral("%1 ref %2 $%3").arg(note.value().id, saleId).arg(amount, 0, 'f', 0));
    return note;
}
