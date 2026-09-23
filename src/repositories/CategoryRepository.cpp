#include "CategoryRepository.h"

#include <QSqlError>
#include <QSqlQuery>

CategoryRepository::CategoryRepository(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

Category CategoryRepository::rowToCategory(const QSqlQuery &q)
{
    Category c;
    c.id = q.value(QStringLiteral("id")).toInt();
    c.name = q.value(QStringLiteral("name")).toString();
    c.parentId = q.value(QStringLiteral("parent_id")).toInt();
    c.businessType = q.value(QStringLiteral("business_type")).toString();
    c.sortOrder = q.value(QStringLiteral("sort_order")).toInt();
    return c;
}

QList<Category> CategoryRepository::list(const QString &businessType) const
{
    QList<Category> out;
    QSqlQuery q(m_db);
    if (businessType.trimmed().isEmpty()) {
        q.exec(QStringLiteral(
            "SELECT * FROM categories ORDER BY parent_id, sort_order, name"));
    } else {
        q.prepare(QStringLiteral(
            "SELECT * FROM categories WHERE business_type IN ('',?) "
            "ORDER BY parent_id, sort_order, name"));
        q.addBindValue(businessType.trimmed());
        q.exec();
    }
    while (q.next())
        out << rowToCategory(q);
    return out;
}

QList<Category> CategoryRepository::childrenOf(int parentId,
                                               const QString &businessType) const
{
    QList<Category> out;
    for (const Category &c : list(businessType)) {
        if (c.parentId == parentId)
            out << c;
    }
    return out;
}

std::optional<Category> CategoryRepository::findById(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM categories WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToCategory(q);
    return std::nullopt;
}

Result<Category> CategoryRepository::add(const QString &name, int parentId,
                                         const QString &businessType, int sortOrder)
{
    const QString clean = name.trimmed();
    if (clean.size() < 2)
        return Result<Category>::failure(QStringLiteral("Nombre mínimo 2 caracteres"));
    if (parentId != 0 && !findById(parentId))
        return Result<Category>::failure(QStringLiteral("Categoría padre no existe"));
    QSqlQuery q(m_db);
    // parent_id 0 = raíz (se guarda 0, no NULL: UNIQUE ignora NULLs duplicados).
    q.prepare(QStringLiteral(
        "INSERT INTO categories (name, parent_id, business_type, sort_order) VALUES (?,?,?,?)"));
    q.addBindValue(clean);
    q.addBindValue(parentId);
    q.addBindValue(businessType.trimmed());
    q.addBindValue(sortOrder);
    if (!q.exec()) {
        if (q.lastError().text().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive))
            return Result<Category>::failure(
                QStringLiteral("Categoría '%1' ya existe en ese nivel").arg(clean));
        return Result<Category>::failure(q.lastError().text());
    }
    return Result<Category>::success(*findById(q.lastInsertId().toInt()));
}

StatusResult CategoryRepository::rename(int id, const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.size() < 2)
        return StatusResult::failure(QStringLiteral("Nombre mínimo 2 caracteres"));
    const auto cur = findById(id);
    if (!cur)
        return StatusResult::failure(QStringLiteral("Categoría no encontrada"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE categories SET name=? WHERE id=?"));
    q.addBindValue(clean);
    q.addBindValue(id);
    if (!q.exec()) {
        if (q.lastError().text().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive))
            return StatusResult::failure(
                QStringLiteral("Categoría '%1' ya existe en ese nivel").arg(clean));
        return StatusResult::failure(q.lastError().text());
    }
    return StatusResult::success({});
}

StatusResult CategoryRepository::remove(int id)
{
    const auto cur = findById(id);
    if (!cur)
        return StatusResult::failure(QStringLiteral("Categoría no encontrada"));
    QSqlQuery kids(m_db);
    kids.prepare(QStringLiteral("SELECT COUNT(*) FROM categories WHERE parent_id=?"));
    kids.addBindValue(id);
    if (kids.exec() && kids.next() && kids.value(0).toInt() > 0)
        return StatusResult::failure(
            QStringLiteral("Tiene subcategorías: elimínelas primero"));
    // Productos que usan ese nombre como cat o subcat (texto plano legacy).
    QSqlQuery used(m_db);
    used.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM products WHERE cat=? OR subcat=?"));
    used.addBindValue(cur->name);
    used.addBindValue(cur->name);
    if (used.exec() && used.next() && used.value(0).toInt() > 0)
        return StatusResult::failure(
            QStringLiteral("Hay productos en '%1': recategorícelos primero").arg(cur->name));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM categories WHERE id=?"));
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("No se pudo eliminar"));
    return StatusResult::success({});
}
