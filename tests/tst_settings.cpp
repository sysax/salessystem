// SettingsService + SettingsRepository: persistencia, defaults y validación.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/SettingsRepository.h"
#include "services/SettingsService.h"

#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

class TstSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsOnFreshDb()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("settings.db"))));
        SettingsRepository repo(dbm.database());
        EventBus bus;
        SettingsService svc(&repo, &bus);
        const QVariantMap m = svc.all();
        QCOMPARE(m[QStringLiteral("business_name")].toString(), QStringLiteral("Mi Negocio"));
        QCOMPARE(m[QStringLiteral("business_type")].toString(),
                 QStringLiteral("miscelanea"));
        QCOMPARE(m[QStringLiteral("currency_symbol")].toString(), QStringLiteral("$"));
        QCOMPARE(m[QStringLiteral("currency_decimals")].toString(), QStringLiteral("0"));
        QCOMPARE(svc.businessName(), QStringLiteral("Mi Negocio"));
        QCOMPARE(svc.currencyDecimals(), 0);
        QVERIFY(!svc.requireExpiry());
    }

    void saveRoundtrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("settings2.db"))));
        SettingsRepository repo(dbm.database());
        EventBus bus;
        int published = 0;
        bus.subscribe(EventBus::SettingsChanged,
                      [&](const EventBus::Payload &) { ++published; });
        SettingsService svc(&repo, &bus);
        QSignalSpy spy(&svc, &SettingsService::settingsChanged);

        auto r = svc.save({{QStringLiteral("business_name"), QStringLiteral("Farmacia La Salud")},
                           {QStringLiteral("business_nit"), QStringLiteral("900123456-7")},
                           {QStringLiteral("currency_symbol"), QStringLiteral("$")},
                           {QStringLiteral("currency_decimals"), QStringLiteral("0")}});
        QVERIFY(r[QStringLiteral("ok")].toBool());
        QCOMPARE(svc.businessName(), QStringLiteral("Farmacia La Salud"));
        QCOMPARE(svc.nit(), QStringLiteral("900123456-7"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(published, 1);
    }

    void rejectsInvalid()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("settings3.db"))));
        SettingsRepository repo(dbm.database());
        SettingsService svc(&repo, nullptr);
        QVERIFY(!svc.save({{QStringLiteral("business_name"), QStringLiteral("")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("currency_decimals"), QStringLiteral("5")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("business_type"), QStringLiteral("nave_espacial")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("tax_rates_json"), QStringLiteral("no-json")}})
                     [QStringLiteral("ok")]
                         .toBool());
        // El nombre válido previo se conserva tras rechazos
        QCOMPARE(svc.businessName(), QStringLiteral("Mi Negocio"));
    }

    void legacyDbWithoutKeysGetsDefaults()
    {
        // Simula BD anterior: borra claves nuevas y verifica defaults en memoria.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("settings4.db"))));
        QSqlQuery q(dbm.database());
        QVERIFY(q.exec(QStringLiteral("DELETE FROM settings WHERE key='business_name'")));
        SettingsRepository repo(dbm.database());
        SettingsService svc(&repo, nullptr);
        QCOMPARE(svc.businessName(), QStringLiteral("Mi Negocio"));
    }
};

QTEST_MAIN(TstSettings)
#include "tst_settings.moc"
