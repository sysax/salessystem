// AuthService contra BD temporal sembrada (sql/seed.sql).
// Incluye prueba de compatibilidad cripto: los hashes PBKDF2 generados
// por Python (hashlib, 100k) deben verificar en C++ (QPasswordDigestor).
#include <QtTest>

#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "services/AuthService.h"
#include "services/Totp.h"

#include <QSqlQuery>
#include <QTemporaryDir>

class TstAuth : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_db = new DatabaseManager(this);
        QVERIFY(m_db->initialize(m_tmp.filePath(QStringLiteral("auth.db"))));
        // Seed aplicado íntegro (sql/seed.sql)
        QCOMPARE(m_db->tableRowCount("users"), 5);
        QCOMPARE(m_db->tableRowCount("products"), 10);
        QCOMPARE(m_db->tableRowCount("clients"), 5);
        QCOMPARE(m_db->tableRowCount("suppliers"), 5);
        QCOMPARE(m_db->tableRowCount("sales"), 8);
        QCOMPARE(m_db->tableRowCount("promos"), 6);
        QCOMPARE(m_db->tableRowCount("counters"), 5);
        m_bus = new EventBus(this);
        m_auth = new AuthService(m_db->database(), m_bus, this);
    }

    // — Compatibilidad con hashes Python —
    void pythonHashesVerify()
    {
        QSqlQuery q(m_db->database());
        q.prepare(QStringLiteral("SELECT password FROM users WHERE username=?"));
        const QList<QPair<QString, QString>> creds = {
            {QStringLiteral("admin"), QStringLiteral("admin123")},
            {QStringLiteral("vendedor"), QStringLiteral("venta123")},
            {QStringLiteral("cajero"), QStringLiteral("caja123")},
            {QStringLiteral("almacen"), QStringLiteral("alma123")},
            {QStringLiteral("contador"), QStringLiteral("conta123")},
        };
        for (const auto &[user, pass] : creds) {
            q.addBindValue(user);
            QVERIFY(q.exec() && q.next());
            QVERIFY2(AuthService::verifyPassword(q.value(0).toString(), pass),
                     qPrintable(QStringLiteral("hash Python de %1 no verifica").arg(user)));
            QVERIFY(!AuthService::verifyPassword(q.value(0).toString(), pass + QStringLiteral("X")));
            q.finish();
        }
    }

    void loginOk()
    {
        const auto r = m_auth->login(QStringLiteral("admin"), QStringLiteral("admin123"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().username, QStringLiteral("admin"));
        QCOMPARE(r.value().role, QStringLiteral("Administrador"));
        QVERIFY(!r.value().totpRequired);
    }

    void loginUnknownAndWrong()
    {
        QVERIFY(!m_auth->login(QStringLiteral("nadie"), QStringLiteral("x")).ok());
        QVERIFY(!m_auth->login(QStringLiteral("admin"), QStringLiteral("mala")).ok());
    }

    void lockoutAfter3()
    {
        QVERIFY(m_auth->addUser(QStringLiteral("locktest"), QStringLiteral("lock1234"),
                                QStringLiteral("Vendedor")).ok());
        QVERIFY(!m_auth->login(QStringLiteral("locktest"), QStringLiteral("mala")).ok());
        QVERIFY(!m_auth->login(QStringLiteral("locktest"), QStringLiteral("mala")).ok());
        QVERIFY(!m_auth->login(QStringLiteral("locktest"), QStringLiteral("mala")).ok());
        // Aun con clave correcta sigue bloqueado
        const auto r = m_auth->login(QStringLiteral("locktest"), QStringLiteral("lock1234"));
        QVERIFY(!r.ok());
        QVERIFY(r.error().contains(QStringLiteral("Bloqueado")));
        // Desbloquear resetea
        QVERIFY(m_auth->setUserActive(QStringLiteral("locktest"), true).ok());
        QVERIFY(m_auth->login(QStringLiteral("locktest"), QStringLiteral("lock1234")).ok());
        QVERIFY(m_auth->deleteUser(QStringLiteral("locktest")).ok());
    }

    void inactiveBlocked()
    {
        QVERIFY(m_auth->addUser(QStringLiteral("inact"), QStringLiteral("inact1234"),
                                QStringLiteral("Cajero")).ok());
        QVERIFY(m_auth->setUserActive(QStringLiteral("inact"), false).ok());
        QVERIFY(!m_auth->login(QStringLiteral("inact"), QStringLiteral("inact1234")).ok());
        QVERIFY(m_auth->deleteUser(QStringLiteral("inact")).ok());
    }

    void userAdminRules()
    {
        QVERIFY(!m_auth->addUser(QStringLiteral("admin"), QStringLiteral("otra1234"),
                                 QStringLiteral("Cajero")).ok()); // duplicado
        QVERIFY(!m_auth->addUser(QStringLiteral("nuevo"), QStringLiteral("abc"),
                                 QStringLiteral("Cajero")).ok()); // corta
        QVERIFY(!m_auth->addUser(QStringLiteral("nuevo"), QStringLiteral("abcd1234"),
                                 QStringLiteral("Super")).ok()); // rol inválido
        QVERIFY(!m_auth->setUserActive(QStringLiteral("admin"), false).ok()); // protege admin
        QVERIFY(!m_auth->deleteUser(QStringLiteral("admin")).ok());
        QVERIFY(!m_auth->resetPassword(QStringLiteral("admin"), QStringLiteral("ab")).ok());
        // reset válido + login con nueva clave
        QVERIFY(m_auth->addUser(QStringLiteral("tmp1"), QStringLiteral("tmp11234"),
                                QStringLiteral("Cajero")).ok());
        QVERIFY(m_auth->resetPassword(QStringLiteral("tmp1"), QStringLiteral("nueva1234")).ok());
        QVERIFY(m_auth->login(QStringLiteral("tmp1"), QStringLiteral("nueva1234")).ok());
        QVERIFY(!m_auth->login(QStringLiteral("tmp1"), QStringLiteral("tmp11234")).ok());
        QVERIFY(m_auth->deleteUser(QStringLiteral("tmp1")).ok());
    }

    void plaintextMigratesOnLogin()
    {
        QSqlQuery q(m_db->database());
        q.prepare(QStringLiteral(
            "INSERT INTO users (username, password, role, active, failed_attempts, created_at) "
            "VALUES (?,?,?,?,?,?)"));
        q.addBindValue(QStringLiteral("legacy"));
        q.addBindValue(QStringLiteral("claveplana")); // texto plano (BD antigua)
        q.addBindValue(QStringLiteral("Cajero"));
        q.addBindValue(1);
        q.addBindValue(0);
        q.addBindValue(QStringLiteral("2024-01-01T00:00:00"));
        QVERIFY(q.exec());
        QVERIFY(m_auth->login(QStringLiteral("legacy"), QStringLiteral("claveplana")).ok());
        // Ahora el hash debe ser PBKDF2 (contiene $)
        QSqlQuery q2(m_db->database());
        q2.prepare(QStringLiteral("SELECT password FROM users WHERE username=?"));
        q2.addBindValue(QStringLiteral("legacy"));
        QVERIFY(q2.exec() && q2.next());
        QVERIFY(q2.value(0).toString().contains(u'$'));
        QVERIFY(m_auth->deleteUser(QStringLiteral("legacy")).ok());
    }

    void twoFactorFlow()
    {
        QVERIFY(m_auth->addUser(QStringLiteral("fa2"), QStringLiteral("fa21234"),
                                QStringLiteral("Vendedor")).ok());
        QVERIFY(!m_auth->is2faEnabled(QStringLiteral("fa2")));
        const auto sec = m_auth->enable2fa(QStringLiteral("fa2"));
        QVERIFY(sec.ok());
        QVERIFY(!sec.value().isEmpty());
        // Confirmar con código erróneo falla
        QVERIFY(!m_auth->confirm2fa(QStringLiteral("fa2"), QStringLiteral("000000")).ok());
        // Confirmar con código vigente → retorna 8 recovery codes
        const auto conf = m_auth->confirm2fa(QStringLiteral("fa2"), Totp::currentCode(sec.value()));
        QVERIFY(conf.ok());
        QCOMPARE(conf.value().size(), 8);
        QVERIFY(m_auth->is2faEnabled(QStringLiteral("fa2")));
        QCOMPARE(m_auth->recoveryCodesLeft(QStringLiteral("fa2")), 8);
        // TOTP vigente verifica
        QVERIFY(m_auth->verify2fa(QStringLiteral("fa2"), Totp::currentCode(sec.value())));
        // Recovery code se consume (un solo uso)
        const QString rec = conf.value().first();
        QVERIFY(m_auth->verify2fa(QStringLiteral("fa2"), rec));
        QCOMPARE(m_auth->recoveryCodesLeft(QStringLiteral("fa2")), 7);
        QVERIFY(!m_auth->verify2fa(QStringLiteral("fa2"), rec));
        // Regenerar repone 8
        QVERIFY(m_auth->regenerateRecoveryCodes(QStringLiteral("fa2")).ok());
        QCOMPARE(m_auth->recoveryCodesLeft(QStringLiteral("fa2")), 8);
        // Desactivar limpia
        QVERIFY(m_auth->disable2fa(QStringLiteral("fa2")).ok());
        QVERIFY(!m_auth->is2faEnabled(QStringLiteral("fa2")));
        QVERIFY(m_auth->deleteUser(QStringLiteral("fa2")).ok());
    }

    void recoveryFlow()
    {
        QVERIFY(m_auth->addUser(QStringLiteral("rec1"), QStringLiteral("rec11234"),
                                QStringLiteral("Cajero")).ok());
        const auto tok = m_auth->requestRecovery(QStringLiteral("rec1"));
        QVERIFY(tok.ok());
        QCOMPARE(tok.value().size(), 6);
        QVERIFY(!m_auth->redeemRecovery(QStringLiteral("rec1"), QStringLiteral("FFFFFF"),
                                        QStringLiteral("nueva1234")).ok());
        QVERIFY(!m_auth->redeemRecovery(QStringLiteral("rec1"), tok.value(),
                                        QStringLiteral("ab")).ok()); // corta
        QVERIFY(m_auth->redeemRecovery(QStringLiteral("rec1"), tok.value(),
                                       QStringLiteral("nueva1234")).ok());
        // Un solo uso
        QVERIFY(!m_auth->redeemRecovery(QStringLiteral("rec1"), tok.value(),
                                        QStringLiteral("otra1234")).ok());
        QVERIFY(m_auth->login(QStringLiteral("rec1"), QStringLiteral("nueva1234")).ok());
        QVERIFY(!m_auth->requestRecovery(QStringLiteral("nadie")).ok());
        QVERIFY(m_auth->deleteUser(QStringLiteral("rec1")).ok());
    }

    void permissionsMatrix()
    {
        QVERIFY(m_auth->canAccess(QStringLiteral("Administrador"), QStringLiteral("users")));
        QVERIFY(m_auth->canAccess(QStringLiteral("Vendedor"), QStringLiteral("pos")));
        QVERIFY(!m_auth->canAccess(QStringLiteral("Vendedor"), QStringLiteral("users")));
        QVERIFY(m_auth->canAccess(QStringLiteral("Cajero"), QStringLiteral("pos")));
        QVERIFY(!m_auth->canAccess(QStringLiteral("Cajero"), QStringLiteral("products")));
        QVERIFY(m_auth->canAccess(QStringLiteral("Almacén"), QStringLiteral("suppliers")));
        QVERIFY(!m_auth->canAccess(QStringLiteral("Almacén"), QStringLiteral("pos")));
        QVERIFY(m_auth->canAccess(QStringLiteral("Contador"), QStringLiteral("reports")));
        QVERIFY(!m_auth->canAccess(QStringLiteral("Contador"), QStringLiteral("pos")));
        QVERIFY(!m_auth->canAccess(QStringLiteral("Fantasma"), QStringLiteral("pos")));
    }

    void eventsEmitted()
    {
        int okCount = 0, failCount = 0;
        m_bus->subscribe(EventBus::UserLoggedIn, [&](const QVariantMap &) { ++okCount; });
        m_bus->subscribe(EventBus::UserLoginFailed, [&](const QVariantMap &) { ++failCount; });
        m_auth->login(QStringLiteral("admin"), QStringLiteral("admin123"));
        m_auth->login(QStringLiteral("admin"), QStringLiteral("mala"));
        QCOMPARE(okCount, 1);
        QCOMPARE(failCount, 1);
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_db = nullptr;
    EventBus *m_bus = nullptr;
    AuthService *m_auth = nullptr;
};

QTEST_MAIN(TstAuth)
#include "tst_auth.moc"
