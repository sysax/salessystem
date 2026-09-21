#include "ClientRepository.h"

#include <QSqlError>
#include <QSqlQuery>

ClientRepository::ClientRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

Client ClientRepository::rowToClient(const QSqlQuery &q)
{
    Client c;
    c.id = q.value(QStringLiteral("id")).toInt();
    c.name = q.value(QStringLiteral("name")).toString();
    c.nit = q.value(QStringLiteral("nit")).toString();
    c.razon = q.value(QStringLiteral("razon")).toString();
    c.regimen = q.value(QStringLiteral("regimen")).toString();
    c.responsabilidad = q.value(QStringLiteral("responsabilidad")).toString();
    c.email = q.value(QStringLiteral("email")).toString();
    c.phone = q.value(QStringLiteral("phone")).toString();
    c.address = q.value(QStringLiteral("address")).toString();
    c.city = q.value(QStringLiteral("city")).toString();
    c.credit = q.value(QStringLiteral("credit")).toDouble();
    c.creditLimit = q.value(QStringLiteral("credit_limit")).toDouble();
    c.discount = q.value(QStringLiteral("discount")).toInt();
    c.balance = q.value(QStringLiteral("balance")).toDouble();
    c.priceList = q.value(QStringLiteral("price_list")).toString();
    c.status = q.value(QStringLiteral("status")).toString();
    return c;
}

QList<Client> ClientRepository::list() const
{
    QList<Client> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM clients ORDER BY id")))
        return out;
    while (q.next())
        out << rowToClient(q);
    return out;
}

std::optional<Client> ClientRepository::findById(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM clients WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToClient(q);
    return std::nullopt;
}

std::optional<Client> ClientRepository::findByName(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM clients WHERE lower(name)=lower(?)"));
    q.addBindValue(name.trimmed());
    if (q.exec() && q.next())
        return rowToClient(q);
    return std::nullopt;
}

QList<Client> ClientRepository::search(const QString &text) const
{
    const QString t = text.trimmed().toLower();
    QList<Client> out;
    QSqlQuery q(m_db);
    if (t.isEmpty()) {
        if (q.exec(QStringLiteral("SELECT * FROM clients ORDER BY id")))
            while (q.next())
                out << rowToClient(q);
        return out;
    }
    q.prepare(QStringLiteral(
        "SELECT * FROM clients WHERE lower(name) LIKE ? OR lower(nit) LIKE ? "
        "OR lower(email) LIKE ? ORDER BY id"));
    const QString like = u'%' + t + u'%';
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToClient(q);
    return out;
}

Result<Client> ClientRepository::add(const Client &cin)
{
    Client c = cin;
    c.name = c.name.trimmed();
    if (c.name.size() < 2)
        return Result<Client>::failure(QStringLiteral("Nombre mínimo 2 caracteres"));
    if (findByName(c.name))
        return Result<Client>::failure(QStringLiteral("Cliente ya existe"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO clients (name, nit, rfc, nit_dv, razon, regimen, responsabilidad, email, "
        "phone, address, city, credit, credit_limit, discount, balance, price_list, status) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(c.name);
    q.addBindValue(c.nit);
    q.addBindValue(c.nit);
    q.addBindValue(c.nit.contains(u'-') ? c.nit.split(u'-').last() : QString());
    q.addBindValue(c.razon.isEmpty() ? c.name : c.razon);
    q.addBindValue(c.regimen.isEmpty() ? QStringLiteral("No responsable IVA") : c.regimen);
    q.addBindValue(c.responsabilidad.isEmpty() ? c.regimen : c.responsabilidad);
    q.addBindValue(c.email);
    q.addBindValue(c.phone);
    q.addBindValue(c.address);
    q.addBindValue(c.city);
    q.addBindValue(c.credit);
    q.addBindValue(c.creditLimit);
    q.addBindValue(c.discount);
    q.addBindValue(c.balance);
    q.addBindValue(c.priceList.isEmpty() ? QStringLiteral("detal") : c.priceList);
    q.addBindValue(c.status.isEmpty() ? QStringLiteral("activo") : c.status);
    if (!q.exec())
        return Result<Client>::failure(q.lastError().text());
    return Result<Client>::success(*findByName(c.name));
}

Result<Client> ClientRepository::update(int id, const Client &c)
{
    if (!findById(id))
        return Result<Client>::failure(
            QStringLiteral("Cliente ID %1 no encontrado").arg(id));
    if (c.discount < 0 || c.discount > 100)
        return Result<Client>::failure(QStringLiteral("Descuento 0-100"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE clients SET name=?, nit=?, rfc=?, email=?, phone=?, city=?, address=?, "
        "credit_limit=?, discount=?, status=?, price_list=?, regimen=?, responsabilidad=? "
        "WHERE id=?"));
    q.addBindValue(c.name);
    q.addBindValue(c.nit);
    q.addBindValue(c.nit);
    q.addBindValue(c.email);
    q.addBindValue(c.phone);
    q.addBindValue(c.city);
    q.addBindValue(c.address);
    q.addBindValue(c.creditLimit);
    q.addBindValue(c.discount);
    q.addBindValue(c.status);
    q.addBindValue(c.priceList);
    q.addBindValue(c.regimen);
    q.addBindValue(c.responsabilidad);
    q.addBindValue(id);
    if (!q.exec())
        return Result<Client>::failure(q.lastError().text());
    return Result<Client>::success(*findById(id));
}

StatusResult ClientRepository::remove(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM clients WHERE id=?"));
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(
            QStringLiteral("Cliente ID %1 no encontrado").arg(id));
    return StatusResult::success({});
}

bool ClientRepository::addCredit(const QString &name, double amount)
{
    const auto c = findByName(name);
    if (!c)
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE clients SET credit=?, balance=? WHERE lower(name)=lower(?)"));
    q.addBindValue(c->credit + amount);
    q.addBindValue(c->balance + amount);
    q.addBindValue(name.trimmed());
    return q.exec();
}

bool ClientRepository::payCredit(const QString &name, double amount)
{
    const auto c = findByName(name);
    if (!c)
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE clients SET credit=?, balance=? WHERE lower(name)=lower(?)"));
    q.addBindValue(std::max(0.0, c->credit - amount));
    q.addBindValue(std::max(0.0, c->balance - amount));
    q.addBindValue(name.trimmed());
    return q.exec();
}

// ── Suppliers ─────────────────────────────────────────────────────────────

SupplierRepository::SupplierRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

Supplier SupplierRepository::rowToSupplier(const QSqlQuery &q)
{
    Supplier s;
    s.id = q.value(QStringLiteral("id")).toInt();
    s.name = q.value(QStringLiteral("name")).toString();
    s.nit = q.value(QStringLiteral("nit")).toString();
    s.contact = q.value(QStringLiteral("contact")).toString();
    s.phone = q.value(QStringLiteral("phone")).toString();
    s.email = q.value(QStringLiteral("email")).toString();
    s.city = q.value(QStringLiteral("city")).toString();
    s.address = q.value(QStringLiteral("address")).toString();
    s.catalog = q.value(QStringLiteral("catalog")).toString();
    s.leadTime = q.value(QStringLiteral("lead_time")).toString();
    s.paymentTerms = q.value(QStringLiteral("payment_terms")).toString();
    s.balance = q.value(QStringLiteral("balance")).toDouble();
    return s;
}

QList<Supplier> SupplierRepository::list() const
{
    QList<Supplier> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM suppliers ORDER BY id")))
        return out;
    while (q.next())
        out << rowToSupplier(q);
    return out;
}

std::optional<Supplier> SupplierRepository::findById(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM suppliers WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToSupplier(q);
    return std::nullopt;
}

std::optional<Supplier> SupplierRepository::findByName(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM suppliers WHERE lower(name)=lower(?)"));
    q.addBindValue(name.trimmed());
    if (q.exec() && q.next())
        return rowToSupplier(q);
    return std::nullopt;
}

QList<Supplier> SupplierRepository::search(const QString &text) const
{
    const QString t = text.trimmed().toLower();
    QList<Supplier> out;
    QSqlQuery q(m_db);
    if (t.isEmpty()) {
        if (q.exec(QStringLiteral("SELECT * FROM suppliers ORDER BY id")))
            while (q.next())
                out << rowToSupplier(q);
        return out;
    }
    q.prepare(QStringLiteral(
        "SELECT * FROM suppliers WHERE lower(name) LIKE ? OR lower(nit) LIKE ? "
        "OR lower(rfc) LIKE ? OR lower(contact) LIKE ? ORDER BY id"));
    const QString like = u'%' + t + u'%';
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToSupplier(q);
    return out;
}

Result<Supplier> SupplierRepository::add(const Supplier &sin)
{
    Supplier s = sin;
    s.name = s.name.trimmed();
    if (s.name.size() < 2)
        return Result<Supplier>::failure(QStringLiteral("Empresa mínimo 2 caracteres"));
    if (findByName(s.name))
        return Result<Supplier>::failure(QStringLiteral("Proveedor ya existe"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO suppliers (name, nit, rfc, contact, phone, email, city, address, catalog, "
        "lead_time, payment_terms, balance) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(s.name);
    q.addBindValue(s.nit);
    q.addBindValue(s.nit);
    q.addBindValue(s.contact);
    q.addBindValue(s.phone);
    q.addBindValue(s.email);
    q.addBindValue(s.city);
    q.addBindValue(s.address);
    q.addBindValue(s.catalog);
    q.addBindValue(s.leadTime);
    q.addBindValue(s.paymentTerms);
    q.addBindValue(s.balance);
    if (!q.exec())
        return Result<Supplier>::failure(q.lastError().text());
    return Result<Supplier>::success(*findByName(s.name));
}

Result<Supplier> SupplierRepository::update(int id, const Supplier &s)
{
    if (!findById(id))
        return Result<Supplier>::failure(
            QStringLiteral("Proveedor ID %1 no encontrado").arg(id));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE suppliers SET name=?, nit=?, rfc=?, contact=?, phone=?, email=?, city=?, "
        "catalog=?, lead_time=?, payment_terms=? WHERE id=?"));
    q.addBindValue(s.name);
    q.addBindValue(s.nit);
    q.addBindValue(s.nit);
    q.addBindValue(s.contact);
    q.addBindValue(s.phone);
    q.addBindValue(s.email);
    q.addBindValue(s.city);
    q.addBindValue(s.catalog);
    q.addBindValue(s.leadTime);
    q.addBindValue(s.paymentTerms);
    q.addBindValue(id);
    if (!q.exec())
        return Result<Supplier>::failure(q.lastError().text());
    return Result<Supplier>::success(*findById(id));
}

StatusResult SupplierRepository::remove(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM suppliers WHERE id=?"));
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(
            QStringLiteral("Proveedor ID %1 no encontrado").arg(id));
    return StatusResult::success({});
}
