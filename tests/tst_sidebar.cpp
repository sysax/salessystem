// Humo QML: el sidebar se puebla al iniciar sesión (regresión:
// antes el modelo se evaluaba una sola vez sin sesión y quedaba vacío).
#include <QtTest>

#include "controllers/AuthController.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "services/AuthService.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QTemporaryDir>

#ifndef QML_DIR
#define QML_DIR ""
#endif

class TstSidebar : public QObject
{
    Q_OBJECT

private slots:
    void menuPopulatesOnLogin()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("sidebar.db"))));
        EventBus bus;
        AuthService authSvc(dbm.database(), &bus);
        AuthController auth(&authSvc);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("auth"), &auth);
        engine.load(QUrl::fromLocalFile(QString::fromLatin1(QML_DIR)
                                        + QStringLiteral("/AppSidebar.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().first();
        QQuickItem *menu =
            root->findChild<QQuickItem *>(QStringLiteral("menuList"));
        QVERIFY(menu != nullptr);

        // Sin sesión: vacío
        QCOMPARE(menu->property("count").toInt(), 0);

        // Login como admin → 13 entradas (acceso total)
        const QVariantMap r =
            auth.login(QStringLiteral("admin"), QStringLiteral("admin123"));
        QVERIFY(r["ok"].toBool());
        QTRY_COMPARE(menu->property("count").toInt(), 13);

        // Logout → vacío de nuevo
        auth.logout();
        QTRY_COMPARE(menu->property("count").toInt(), 0);
    }
};

QTEST_MAIN(TstSidebar)
#include "tst_sidebar.moc"
