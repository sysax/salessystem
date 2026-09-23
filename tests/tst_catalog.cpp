// Fase 2.1: categorías jerárquicas — CRUD, unicidad, filtro por vertical,
// protección contra borrado con hijas o productos.
#include <QtTest>

#include "controllers/CatalogController.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/CategoryRepository.h"
#include "repositories/ProductRepository.h"

#include <QSqlQuery>
#include <QTemporaryDir>

class TstCatalog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("catalog.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, audit, this);
        m_cats = new CategoryRepository(db, this);
        m_ctl = new CatalogController(m_products, m_cats, nullptr, this);
    }

    void crud()
    {
        auto r = m_cats->add(QStringLiteral("Abarrotes"));
        QVERIFY(r.ok());
        const int root = r.value().id;
        auto s = m_cats->add(QStringLiteral("Granos"), root);
        QVERIFY(s.ok());
        // Unicidad por nivel: duplicada falla, en otro nivel pasa
        QVERIFY(!m_cats->add(QStringLiteral("Granos"), root).ok());
        auto other = m_cats->add(QStringLiteral("Raiz2"));
        QVERIFY(other.ok());
        QVERIFY(m_cats->add(QStringLiteral("Granos"), other.value().id).ok());
        // Rename
        QVERIFY(m_cats->rename(s.value().id, QStringLiteral("Granos y cereales")).ok());
        QCOMPARE(m_cats->findById(s.value().id)->name, QStringLiteral("Granos y cereales"));
        // Padre inexistente / nombre corto
        QVERIFY(!m_cats->add(QStringLiteral("X"), 9999).ok());
        QVERIFY(!m_cats->add(QStringLiteral("X")).ok());
    }

    void businessTypeFilter()
    {
        QVERIFY(m_cats->add(QStringLiteral("Medicamentos"), 0, QStringLiteral("farmacia")).ok());
        QVERIFY(m_cats->add(QStringLiteral("Comunes")).ok());
        const auto all = m_cats->list();
        const auto farma = m_cats->list(QStringLiteral("farmacia"));
        const auto talle = m_cats->list(QStringLiteral("taller"));
        // farma ve farmacia+comunes; taller solo comunes
        bool farmaSeesMed = false, tallerSeesMed = false;
        for (const auto &c : farma)
            if (c.name == QStringLiteral("Medicamentos"))
                farmaSeesMed = true;
        for (const auto &c : talle)
            if (c.name == QStringLiteral("Medicamentos"))
                tallerSeesMed = true;
        QVERIFY(farmaSeesMed && !tallerSeesMed);
        QVERIFY(talle.size() < all.size());
        // childrenOf
        const auto roots = m_cats->childrenOf(0);
        QVERIFY(!roots.isEmpty());
    }

    void removeProtected()
    {
        auto r = m_cats->add(QStringLiteral("Protegida"));
        QVERIFY(r.ok());
        auto k = m_cats->add(QStringLiteral("Hija"), r.value().id);
        QVERIFY(k.ok());
        // Con hijas no se puede borrar
        QVERIFY(!m_cats->remove(r.value().id).ok());
        // Con productos no se puede borrar
        Product p;
        p.sku = QStringLiteral("CAT1");
        p.name = QStringLiteral("Prod Cat");
        p.price = 1000.0;
        p.stock = 5;
        p.cat = QStringLiteral("Hija");
        QVERIFY(m_products->add(p).ok());
        QVERIFY(!m_cats->remove(k.value().id).ok());
        // Limpio sí se puede
        QVERIFY(m_cats->add(QStringLiteral("Suelta")).ok());
        const auto all = m_cats->list();
        int suelta = 0;
        for (const auto &c : all)
            if (c.name == QStringLiteral("Suelta"))
                suelta = c.id;
        QVERIFY(suelta != 0);
        QVERIFY(m_cats->remove(suelta).ok());
        QVERIFY(!m_cats->findById(suelta).has_value());
    }

    void seedIdempotent()
    {
        // Fase 2.4: el seed añade sin duplicar y no toca usuarios/ventas.
        QVERIFY(m_dbm->applySeedFile(QStringLiteral("miscelanea")));
        const int cats1 = m_dbm->tableRowCount(QStringLiteral("categories"));
        const int prods1 = m_dbm->tableRowCount(QStringLiteral("products"));
        QVERIFY(cats1 >= 3 && prods1 >= 5);
        QVERIFY(m_dbm->applySeedFile(QStringLiteral("miscelanea")));
        QCOMPARE(m_dbm->tableRowCount(QStringLiteral("categories")), cats1);
        QCOMPARE(m_dbm->tableRowCount(QStringLiteral("products")), prods1);
        QCOMPARE(m_dbm->tableRowCount(QStringLiteral("users")), 1);
        QCOMPARE(m_dbm->tableRowCount(QStringLiteral("sales")), 0);
        // Categorías del seed visibles para esa vertical
        const auto misc = m_cats->list(QStringLiteral("miscelanea"));
        bool hasPapeleria = false;
        for (const auto &c : misc) {
            if (c.name == QStringLiteral("Papelería"))
                hasPapeleria = true;
        }
        QVERIFY(hasPapeleria);
        // Seed inexistente falla sin romper nada
        QVERIFY(!m_dbm->applySeedFile(QStringLiteral("nave_espacial")));
        QVERIFY(!m_dbm->applySeedFile(QStringLiteral("../schema")));
    }

    void controllerReload()
    {
        m_ctl->reloadCategories(QStringLiteral("farmacia"));
        QVERIFY(!m_ctl->categories().isEmpty());
        QVERIFY(m_ctl->addCategory(QStringLiteral("Dermocosmética"), 0,
                                   QStringLiteral("farmacia"))["ok"].toBool());
        QVERIFY(!m_ctl->addCategory(QStringLiteral("X"))["ok"].toBool());
        QVERIFY(!m_ctl->removeCategory(99999)["ok"].toBool());
    }

    void unitValidation()
    {
        // Fase 2: unidades canónicas.
        QVERIFY(ProductRepository::Units.contains(QStringLiteral("kg")));
        QVERIFY(ProductRepository::isWeighable(QStringLiteral("kg")));
        QVERIFY(!ProductRepository::isWeighable(QStringLiteral("caja")));
        QCOMPARE(ProductRepository::normalizeUnit(QStringLiteral("KG")), QStringLiteral("kg"));
        QVERIFY(ProductRepository::normalizeUnit(QStringLiteral("tonelada")).isEmpty());
        Product p;
        p.sku = QStringLiteral("UNIT1");
        p.name = QStringLiteral("Prod Unidad");
        p.price = 1000.0;
        p.stock = 5.0;
        p.unit = QStringLiteral("tonelada");
        QVERIFY(!m_products->add(p).ok()); // alta exige unidad válida
        p.unit = QStringLiteral("");
        QVERIFY(m_products->add(p).ok()); // vacío → "unidad"
        QCOMPARE(m_products->findBySku(QStringLiteral("UNIT1"))->unit,
                 QStringLiteral("unidad"));
        // Decimal stock round-trip
        Product g;
        g.sku = QStringLiteral("UNIT2");
        g.name = QStringLiteral("Granel");
        g.price = 2000.0;
        g.stock = 4.75;
        g.unit = QStringLiteral("kg");
        QVERIFY(m_products->add(g).ok());
        QCOMPARE(m_products->findBySku(QStringLiteral("UNIT2"))->stock, 4.75);
        // Unidad legacy preservada al editar otros campos
        QSqlQuery q(m_dbm->database());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO products (sku, name, price, stock, unit, status) VALUES "
            "('LEGACY1','Viejo',500,3,'pieza','activo')")));
        auto upd = m_products->findBySku(QStringLiteral("LEGACY1"));
        QVERIFY(upd.has_value());
        Product u = *upd;
        u.price = 600.0;
        u.priceBuy = 400.0; // el INSERT crudo dejó price_buy en 0 (update exige >0)
        QVERIFY(m_products->update(QStringLiteral("LEGACY1"), u).ok());
        QCOMPARE(m_products->findBySku(QStringLiteral("LEGACY1"))->unit,
                 QStringLiteral("pieza"));
        u.unit = QStringLiteral("tonelada");
        QVERIFY(!m_products->update(QStringLiteral("LEGACY1"), u).ok());
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    ProductRepository *m_products = nullptr;
    CategoryRepository *m_cats = nullptr;
    CatalogController *m_ctl = nullptr;
};

QTEST_MAIN(TstCatalog)
#include "tst_catalog.moc"
