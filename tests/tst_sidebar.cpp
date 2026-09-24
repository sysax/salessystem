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

class MockSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
public:
    QVariantMap settings() const { return m_settings; }
    void setBusinessType(const QString &bt)
    {
        m_settings["business_type"] = bt;
        emit settingsChanged();
    }
signals:
    void settingsChanged();
private:
    QVariantMap m_settings;
};

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

        // Login como admin → 16 entradas (acceso total, incluye Configuración,
        // Lotes y Seriales de Fase 4; sin settingsCtl se muestra todo)
        const QVariantMap r =
            auth.login(QStringLiteral("admin"), QStringLiteral("admin123"));
        QVERIFY(r["ok"].toBool());
        QTRY_COMPARE(menu->property("count").toInt(), 16);

        // Logout → vacío de nuevo
        auth.logout();
        QTRY_COMPARE(menu->property("count").toInt(), 0);
    }

    void menuFiltersByVertical()
    {
        // Fase 4: con settingsCtl, farmacia ve Lotes pero no Seriales.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("sidebar2.db"))));
        EventBus bus;
        AuthService authSvc(dbm.database(), &bus);
        AuthController auth(&authSvc);
        MockSettings settings;

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("auth"), &auth);
        engine.rootContext()->setContextProperty(QStringLiteral("settingsCtl"), &settings);
        engine.load(QUrl::fromLocalFile(QString::fromLatin1(QML_DIR)
                                        + QStringLiteral("/AppSidebar.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().first();
        QQuickItem *menu =
            root->findChild<QQuickItem *>(QStringLiteral("menuList"));
        QVERIFY(menu != nullptr);

        QVERIFY(auth.login(QStringLiteral("admin"), QStringLiteral("admin123"))["ok"].toBool());
        settings.setBusinessType(QStringLiteral("farmacia"));
        QMetaObject::invokeMethod(root, "refresh", Qt::DirectConnection);
        QTRY_COMPARE(menu->property("count").toInt(), 15); // 14 + lots

        settings.setBusinessType(QStringLiteral("celulares"));
        QMetaObject::invokeMethod(root, "refresh", Qt::DirectConnection);
        QTRY_COMPARE(menu->property("count").toInt(), 15); // 14 + serials

        settings.setBusinessType(QStringLiteral("miscelanea"));
        QMetaObject::invokeMethod(root, "refresh", Qt::DirectConnection);
        QTRY_COMPARE(menu->property("count").toInt(), 14); // solo genéricos
    }
};

QTEST_MAIN(TstSidebar)
#include "tst_sidebar.moc"
