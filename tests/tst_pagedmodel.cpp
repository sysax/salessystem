// Fase 4: PagedListModel — roles desde claves, set/append/clear,
// totalCount y canFetchMore.
#include <QtTest>

#include "controllers/PagedListModel.h"

class TstPagedModel : public QObject
{
    Q_OBJECT

  private slots:
    void rolesFromKeys()
    {
        PagedListModel m;
        QCOMPARE(m.rowCount(), 0);
        QVERIFY(!m.canFetchMore());
        m.setRows({QVariantMap{{"sku", "A"}, {"name", "Uno"}}});
        QCOMPARE(m.rowCount(), 1);
        const auto roles = m.roleNames();
        QVERIFY(roles.values().contains("sku"));
        QVERIFY(roles.values().contains("name"));
        const QModelIndex idx = m.index(0, 0);
        QCOMPARE(m.data(idx, roleFor(roles, "sku")).toString(), QStringLiteral("A"));
        QCOMPARE(m.get(0)["name"].toString(), QStringLiteral("Uno"));
        QVERIFY(!m.get(9).contains("sku")); // fuera de rango
    }

    void appendAndTotal()
    {
        PagedListModel m;
        m.setTotalCount(5);
        QVERIFY(m.canFetchMore()); // 0 < 5
        m.appendRows({QVariantMap{{"id", 1}}, QVariantMap{{"id", 2}}});
        QCOMPARE(m.rowCount(), 2);
        QVERIFY(m.canFetchMore());
        m.appendRows({QVariantMap{{"id", 3}}});
        QCOMPARE(m.rowCount(), 3);
        // Llegar al total cierra el fetch.
        m.appendRows({QVariantMap{{"id", 4}}, QVariantMap{{"id", 5}}});
        QCOMPARE(m.rowCount(), 5);
        QVERIFY(!m.canFetchMore());
        // append vacío no hace nada.
        m.appendRows({});
        QCOMPARE(m.rowCount(), 5);
        // setRows reinicia.
        m.setRows({QVariantMap{{"id", 9}}});
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.get(0)["id"].toInt(), 9);
        m.clear();
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.totalCount(), 0);
        QVERIFY(!m.canFetchMore());
    }

  private:
    static int roleFor(const QHash<int, QByteArray> &roles, const char *name)
    {
        for (auto it = roles.begin(); it != roles.end(); ++it) {
            if (it.value() == name)
                return it.key();
        }
        return -1;
    }
};

QTEST_MAIN(TstPagedModel)
#include "tst_pagedmodel.moc"
