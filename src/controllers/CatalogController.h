#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/ProductRepository.h"

// Catálogo ABM + búsqueda (antes ProductsScreen). Los mapas usan las
// mismas claves que la BD para enlace directo con formularios QML.
class CatalogController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList products READ products NOTIFY productsChanged)

public:
    explicit CatalogController(ProductRepository *products, QObject *parent = nullptr);

    QVariantList products() const { return m_products; }

    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(const QString &sku, const QVariantMap &fields);
    Q_INVOKABLE QVariantMap remove(const QString &sku);

    static QVariantMap toMap(const Product &p);
    static Product fromMap(const QVariantMap &m, const Product &base = {});

signals:
    void productsChanged();

private:
    ProductRepository *m_repos = nullptr;
    QVariantList m_products;
};
