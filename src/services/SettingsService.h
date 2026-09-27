#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "../core/Money.h"
#include "../core/Result.h"

class EventBus;
class SettingsRepository;

// Capa Services (arquitectura.txt): configuración del negocio (Fase 0).
// Expone Q_PROPERTY para QML vía SettingsController y valida antes de
// persistir. Emite settingsChanged + publica EventBus::SettingsChanged.
class SettingsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString businessName READ businessName NOTIFY settingsChanged)
    Q_PROPERTY(QString businessType READ businessType NOTIFY settingsChanged)
    Q_PROPERTY(QString nit READ nit NOTIFY settingsChanged)
    Q_PROPERTY(QString address READ address NOTIFY settingsChanged)
    Q_PROPERTY(QString phone READ phone NOTIFY settingsChanged)
    Q_PROPERTY(QString currencySymbol READ currencySymbol NOTIFY settingsChanged)
    Q_PROPERTY(int currencyDecimals READ currencyDecimals NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultTaxRate READ defaultTaxRate NOTIFY settingsChanged)
    Q_PROPERTY(bool requireExpiry READ requireExpiry NOTIFY settingsChanged)
    Q_PROPERTY(bool requireSerial READ requireSerial NOTIFY settingsChanged)

  public:
    struct TaxRate
    {
        QString name;
        double rate = 0.0;
    };
    // Fase 3: serie de folio por tipo documental (prefijo + contador).
    struct FolioSerie
    {
        QString prefix;
        QString counter;
    };
    // Claves canónicas (tabla `settings`, ver sql/schema.sql).
    static const QString KBusinessName;
    static const QString KBusinessType;
    static const QString KNit;
    static const QString KAddress;
    static const QString KPhone;
    static const QString KLogoPath;
    static const QString KCurrencySymbol;
    static const QString KCurrencyDecimals;
    static const QString KDefaultTaxRate;
    static const QString KTaxRatesJson;
    static const QString KMoraRate;
    static const QString KRequireExpiry;
    static const QString KRequireSerial;
    static const QString KWeightUnit;
    // Fase 3 (MAP_PRO): configuración externalizada de crédito y operación.
    static const QString KCreditDays;
    static const QString KPayableDays;
    static const QString KDefaultCreditLimit;
    static const QString KFixedCostsMonthly;
    static const QString KPromoVolumenMinQty;
    static const QString KFolioSeriesJson;

    static const QStringList BusinessTypes;

    explicit SettingsService(SettingsRepository *repo, EventBus *bus = nullptr,
                             QObject *parent = nullptr);

    QString businessName() const;
    QString businessType() const;
    QString nit() const;
    QString address() const;
    QString phone() const;
    QString logoPath() const;
    QString currencySymbol() const;
    int currencyDecimals() const;
    QString defaultTaxRate() const;
    QString taxRatesJson() const;
    // Fase 1: tasas parseadas (fallback {IVA 19%, Excluido} si el JSON es inválido).
    QList<TaxRate> taxRates() const;
    double defaultTaxRateValue() const;
    QString taxNameForRate(double rate) const;
    double moraRate() const;
    bool requireExpiry() const;
    bool requireSerial() const;
    QString weightUnit() const;
    // Fase 3: crédito y operación (con defaults si la clave falta o es inválida).
    int creditDays() const;
    int payableDays() const;
    Money defaultCreditLimit() const;
    Money fixedCostsMonthly() const;
    int promoVolumenMinQty() const;
    QString folioSeriesJson() const;
    // JSON inválido → series por defecto (las de SaleRepository::createDocument).
    QMap<QString, FolioSerie> folioSeries() const;
    static QMap<QString, FolioSerie> defaultFolioSeries();
    static QString defaultFolioSeriesJson();

    Q_INVOKABLE QVariantMap all() const;
    // Valida y persiste; retorna {ok, error}. En éxito emite
    // settingsChanged y publica en el bus.
    Q_INVOKABLE QVariantMap save(const QVariantMap &m);

  signals:
    void settingsChanged();

  private:
    QString validate(const QVariantMap &m, QVariantMap &cleaned) const;

    SettingsRepository *m_repo = nullptr;
    EventBus *m_bus = nullptr;
};
