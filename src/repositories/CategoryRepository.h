#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include <optional>

#include "../core/Result.h"

// Fase 2: diccionario de categorías jerárquicas (categories). Los productos
// guardan cat/subcat como texto (compat legacy); esta tabla alimenta combos
// y filtros. business_type '' = aplica a todas las verticales.
struct Category {
    int id = 0;
    QString name;
    int parentId = 0; // 0 = raíz
    QString businessType;
    int sortOrder = 0;
};

class CategoryRepository : public QObject
{
    Q_OBJECT

public:
    explicit CategoryRepository(QSqlDatabase db, QObject *parent = nullptr);

    // businessType '' = todas; si no, filtra business_type IN ('', bt).
    QList<Category> list(const QString &businessType = {}) const;
    QList<Category> childrenOf(int parentId, const QString &businessType = {}) const;
    std::optional<Category> findById(int id) const;

    Result<Category> add(const QString &name, int parentId = 0,
                         const QString &businessType = {}, int sortOrder = 0);
    StatusResult rename(int id, const QString &name);
    // Falla si tiene hijas o si hay productos con ese cat/subcat.
    StatusResult remove(int id);

    static Category rowToCategory(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
};
