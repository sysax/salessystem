#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/CategoryRepository.h"
#include "../repositories/ProductRepository.h"
#include "../services/SettingsService.h"
#include "PagedListModel.h"

// Catálogo ABM + búsqueda (antes ProductsScreen). Los mapas usan las
// mismas claves que la BD para enlace directo con formularios QML.
class CatalogController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList products READ products NOTIFY productsChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY productsChanged)
    Q_PROPERTY(PagedListModel *productModel READ productModel CONSTANT)

  public:
    explicit CatalogController(ProductRepository *products,
                               CategoryRepository *categories = nullptr,
                               SettingsService *settings = nullptr, QObject *parent = nullptr);

    QVariantList products() const
    {
        return m_products;
    }
    int totalCount() const
    {
        return m_totalCount;
    }
    PagedListModel *productModel()
    {
        return &m_productModel;
    }
    QVariantList categories() const
    {
        return m_categories;
    }

    Q_INVOKABLE void search(const QString &text, const QString &businessType = {});
    // Fase 4: página servidor (page 0-based; pageSize <= 0 = todo).
    Q_INVOKABLE void searchPaged(const QString &text, const QString &businessType, int page,
                                 int pageSize, const QString &sortKey = {}, bool sortAsc = true);
    // Fase 4: scroll infinito sobre el modelo incremental (página fija 30).
    Q_INVOKABLE void searchProducts(const QString &text, const QString &sortKey = {},
                                    bool sortAsc = true);
    Q_INVOKABLE void fetchMoreProducts();
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(const QString &sku, const QVariantMap &fields,
                                   const QString &user = {});
    Q_INVOKABLE QVariantMap remove(const QString &sku);

    // Fase 2: diccionario de categorías (businessType '' = todas).
    Q_INVOKABLE void reloadCategories(const QString &businessType = {});
    Q_INVOKABLE QVariantMap addCategory(const QString &name, int parentId = 0,
                                        const QString &businessType = {});
    Q_INVOKABLE QVariantMap removeCategory(int id);
    // Multitienda: vista previa del cambio de rubro (filtrar sin borrar).
    // Retorna {visibleProducts, hiddenProducts, visibleCategories,
    // hiddenCategories, hiddenSamples[]}. 'miscelanea'/vacío = ve todo.
    Q_INVOKABLE QVariantMap visibilityPreview(const QString &businessType) const;

    static QVariantMap toMap(const Product &p);
    static Product fromMap(const QVariantMap &m, const Product &base = {});
    static QVariantMap categoryToMap(const Category &c);

  signals:
    void productsChanged();
    void categoriesChanged();

  private:
    // Fase 4: re-ejecuta la última consulta (paginada o no) tras mutar.
    void reloadProducts();
    // Multitienda: rubro efectivo (explícito o activo; miscelanea = todo='').
    QString resolvedBt(const QString &businessType = {}) const;

    ProductRepository *m_repos = nullptr;
    CategoryRepository *m_cats = nullptr;
    SettingsService *m_settings = nullptr;
    QString m_catFilter;
    QVariantList m_products;
    QVariantList m_categories;
    int m_totalCount = 0;
    // Última consulta de productos (sticky para add/update/remove).
    QString m_lastText;
    QString m_lastBt;
    int m_lastPage = 0;
    int m_lastSize = 0;
    QString m_lastSortKey;
    bool m_lastSortAsc = true;
    bool m_pagedActive = false;
    // Fase 4: scroll infinito (tamaño de lote servidor).
    PagedListModel m_productModel;
    QString m_modelText;
    QString m_modelSortKey;
    bool m_modelSortAsc = true;
    int m_modelPage = 0;
    bool m_modelActive = false;
    static constexpr int ModelPageSize = 30;
};
