#include "CreditRepository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>

#include "../core/Transaction.h"
#include "ClientRepository.h"

double ReceivablesRepository::MoraRate = 0.02;

ReceivablesRepository::ReceivablesRepository(QSqlDatabase db, SaleRepository *sales,
                                             AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_sales(sales), m_audit(audit)
{
}

QList<Sale> ReceivablesRepository::pending() const
{
    QList<Sale> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM sales WHERE balance>0 AND status NOT IN "
                               "('Cancelada','Cotización','Pedido')")))
        return out;
    while (q.next())
        out << SaleRepository::rowToSale(q);
    return out;
}

QList<Sale> ReceivablesRepository::statement(const QString &clientName) const
{
    QList<Sale> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM sales WHERE client=?"));
    q.addBindValue(clientName);
    if (!q.exec())
        return out;
    while (q.next())
        out << SaleRepository::rowToSale(q);
    return out;
}

QList<CxcPayment> ReceivablesRepository::paymentsFor(const QString &saleId) const
{
    QList<CxcPayment> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM payments_cxc WHERE sale_id=? ORDER BY id"));
    q.addBindValue(saleId);
    if (!q.exec())
        return out;
    while (q.next()) {
        CxcPayment p;
        p.id = q.value(QStringLiteral("id")).toInt();
        p.saleId = q.value(QStringLiteral("sale_id")).toString();
        p.date = q.value(QStringLiteral("date")).toString();
        p.amount = q.value(QStringLiteral("amount")).toDouble();
        p.method = q.value(QStringLiteral("method")).toString();
        p.user = q.value(QStringLiteral("user")).toString();
        out << p;
    }
    return out;
}

Result<Sale> ReceivablesRepository::addPayment(const QString &saleId, double amount,
                                               const QString &method, const QString &user)
{
    if (!m_sales)
        return Result<Sale>::failure(QStringLiteral("Sin acceso a ventas"));
    const auto s = m_sales->find(saleId);
    if (!s)
        return Result<Sale>::failure(QStringLiteral("Venta %1 no encontrada").arg(saleId));
    if (s->balance <= 0)
        return Result<Sale>::failure(QStringLiteral("Venta sin saldo pendiente"));
    if (amount <= 0 || amount > s->balance)
        return Result<Sale>::failure(QStringLiteral("Monto 0 < %1 <= balance %2")
                                         .arg(amount, 0, 'f', 2)
                                         .arg(s->balance, 0, 'f', 2));
    const double newBal = s->balance - amount;
    const bool settled = newBal <= 0.01;
    const QString newStatus = settled ? QStringLiteral("Pagada") : s->status;
    // Fase 1: saldo + abono + crédito del cliente en una transacción
    // (antes, el INSERT en payments_cxc se ejecutaba sin verificar y
    // fuera de transacción: el abono podía perderse en silencio).
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Sale>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sales SET paid=?, balance=?, status=?, estado=? WHERE id=?"));
    q.addBindValue(s->paid + amount);
    q.addBindValue(settled ? 0.0 : newBal);
    q.addBindValue(newStatus);
    q.addBindValue(settled ? newStatus : s->estado);
    q.addBindValue(saleId);
    if (!q.exec())
        return Result<Sale>::failure(q.lastError().text());
    if (settled) {
        ClientRepository clients(m_db);
        if (!clients.payCredit(s->client, amount))
            return Result<Sale>::failure(
                QStringLiteral("No se pudo descargar el crédito del cliente"));
    }
    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO payments_cxc (sale_id, date, amount, method, user) VALUES (?,?,?,?,?)"));
    ins.addBindValue(saleId);
    ins.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    ins.addBindValue(amount);
    ins.addBindValue(method);
    ins.addBindValue(user);
    if (!ins.exec())
        return Result<Sale>::failure(QStringLiteral("Abono: ") + ins.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("cxc_abono"),
                     QStringLiteral("%1 $%2 %3 bal %4")
                         .arg(saleId)
                         .arg(amount, 0, 'f', 0)
                         .arg(method)
                         .arg(settled ? 0.0 : newBal, 0, 'f', 2));
    if (!tx.commit())
        return Result<Sale>::failure(QStringLiteral("No se pudo confirmar el abono"));
    return Result<Sale>::success(*m_sales->find(saleId));
}

double ReceivablesRepository::mora(const Sale &s, double rate)
{
    if (s.balance <= 0)
        return 0.0;
    const QString ref = s.due.isEmpty() ? s.date : s.due;
    const QDate due = QDate::fromString(ref, Qt::ISODate);
    if (!due.isValid())
        return 0.0;
    const QDate today = QDate::currentDate();
    if (today <= due)
        return 0.0;
    const double months = due.daysTo(today) / 30.0;
    return s.balance * rate * months;
}

// ── Payables ──────────────────────────────────────────────────────────────

PayablesRepository::PayablesRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

Payable PayablesRepository::rowToPayable(const QSqlQuery &q)
{
    Payable p;
    p.id = q.value(QStringLiteral("id")).toString();
    p.supplier = q.value(QStringLiteral("supplier")).toString();
    p.due = q.value(QStringLiteral("due")).toString();
    p.amount = q.value(QStringLiteral("amount")).toDouble();
    p.paid = q.value(QStringLiteral("paid")).toDouble();
    p.balance = q.value(QStringLiteral("balance")).toDouble();
    p.discountEarly = q.value(QStringLiteral("discount_early")).toDouble();
    p.status = q.value(QStringLiteral("status")).toString();
    return p;
}

QList<Payable> PayablesRepository::pending() const
{
    QList<Payable> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM payables WHERE balance>0")))
        return out;
    while (q.next())
        out << rowToPayable(q);
    return out;
}

std::optional<Payable> PayablesRepository::find(const QString &id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM payables WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToPayable(q);
    return std::nullopt;
}

StatusResult PayablesRepository::create(const Payable &p)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO payables (id, supplier, due, amount, paid, balance, discount_early, status) "
        "VALUES (?,?,?,?,?,?,?,?)"));
    q.addBindValue(p.id);
    q.addBindValue(p.supplier);
    q.addBindValue(p.due);
    q.addBindValue(p.amount);
    q.addBindValue(p.paid);
    q.addBindValue(p.balance);
    q.addBindValue(p.discountEarly);
    q.addBindValue(p.status);
    if (!q.exec())
        return StatusResult::failure(q.lastError().text());
    return StatusResult::success({});
}

Result<PayablesRepository::PaymentResult> PayablesRepository::addPayment(const QString &payableId,
                                                                         double amount,
                                                                         const QString &method,
                                                                         const QString &user)
{
    const auto p = find(payableId);
    if (!p)
        return Result<PaymentResult>::failure(
            QStringLiteral("CxP %1 no encontrada").arg(payableId));
    if (amount <= 0 || amount > p->balance + 0.01)
        return Result<PaymentResult>::failure(
            QStringLiteral("Monto inválido balance %1").arg(p->balance, 0, 'f', 2));
    const double newBal = p->balance - amount;
    const bool settled = newBal <= 0.01;
    const QString newStatus = settled ? QStringLiteral("Pagada") : p->status;
    // Fase 1: saldo + pago en una transacción (el INSERT antes no se
    // verificaba: el pago podía perderse con el saldo ya descontado).
    Transaction payTx(m_db);
    if (!payTx.isValid())
        return Result<PaymentResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE payables SET paid=?, balance=?, status=? WHERE id=?"));
    q.addBindValue(p->paid + amount);
    q.addBindValue(settled ? 0.0 : newBal);
    q.addBindValue(newStatus);
    q.addBindValue(payableId);
    if (!q.exec())
        return Result<PaymentResult>::failure(q.lastError().text());
    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO payments_cxp (payable_id, date, amount, method, user) VALUES (?,?,?,?,?)"));
    ins.addBindValue(payableId);
    ins.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    ins.addBindValue(amount);
    ins.addBindValue(method);
    ins.addBindValue(user);
    if (!ins.exec())
        return Result<PaymentResult>::failure(QStringLiteral("Pago: ") + ins.lastError().text());
    // Descuento pronto pago (antes del vencimiento)
    double disc = 0.0;
    const QDate due = QDate::fromString(p->due, Qt::ISODate);
    if (due.isValid() && QDate::currentDate() < due && p->discountEarly > 0)
        disc = amount * p->discountEarly / 100.0;
    if (m_audit)
        m_audit->log(user, QStringLiteral("cxp_pago"),
                     QStringLiteral("%1 $%2 %3 bal %4 desc %5")
                         .arg(payableId)
                         .arg(amount, 0, 'f', 0)
                         .arg(method)
                         .arg(settled ? 0.0 : newBal, 0, 'f', 2)
                         .arg(disc, 0, 'f', 2));
    PaymentResult r;
    const auto updated = find(payableId);
    if (!updated)
        return Result<PaymentResult>::failure(
            QStringLiteral("CxP %1 desapareció a mitad del pago").arg(payableId));
    r.payable = *updated;
    r.earlyDiscount = disc;
    if (!payTx.commit())
        return Result<PaymentResult>::failure(QStringLiteral("No se pudo confirmar el pago"));
    return Result<PaymentResult>::success(r);
}
