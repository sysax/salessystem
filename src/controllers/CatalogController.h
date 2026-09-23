#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/CategoryRepository.h"
#include "../repositories/ProductRepository.h"
#include "../services/SettingsService.h"

// Catálogo ABM + búsqueda (antes ProductsScreen). Los mapas usan las
// mismas claves que la BD para enlace directo con formularios QML.
class CatalogController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList products READ products NOTIFY productsChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)

public:
    explicit CatalogController(ProductRepository *products,
                               CategoryRepository *categories = nullptr,
                               SettingsService *settings = nullptr,
                               QObject *parent = nullptr);

    QVariantList products() const { return m_products; }
    QVariantList categories() const { return m_categories; }

    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(const QString &sku, const QVariantMap &fields);
    Q_INVOKABLE QVariantMap remove(const QString &sku);

    // Fase 2: diccionario de categorías (businessType '' = todas).
    Q_INVOKABLE void reloadCategories(const QString &businessType = {});
    Q_INVOKABLE QVariantMap addCategory(const QString &name, int parentId = 0,
                                        const QString &businessType = {});
    Q_INVOKABLE QVariantMap removeCategory(int id);

    static QVariantMap toMap(const Product &p);
    static Product fromMap(const QVariantMap &m, const Product &base = {});
    static QVariantMap categoryToMap(const Category &c);

signals:
    void productsChanged();
    void categoriesChanged();

private:
    ProductRepository *m_repos = nullptr;
    CategoryRepository *m_cats = nullptr;
    SettingsService *m_settings = nullptr;
    QString m_catFilter;
    QVariantList m_products;
    QVariantList m_categories;
};
