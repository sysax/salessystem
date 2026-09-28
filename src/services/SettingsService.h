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
    // Fase 7 (MAP_PRO): idioma de la UI (es|en, es = fuente sin translator).
    static const QString KLanguage;
    // Fase 7 (MAP_PRO): apariencia y operación por operador.
    // theme: light|dark (Material Light/Dark, aplica inmediato).
    static const QString KTheme;
    // high_contrast: 0|1 (paleta de contraste reforzado).
    static const QString KHighContrast;
    // pos_density: compacto|normal|amplio (altura táctil POS 44/48/56).
    static const QString KPosDensity;
    // Atajos de navegación personalizables (secuencias estilo "Ctrl+1").
    static const QString KShortcutDashboard;
    static const QString KShortcutPos;
    static const QString KShortcutProducts;
    static const QString KShortcutSales;
    static const QString KShortcutInventory;
    static const QString KShortcutReports;
    static const QString KShortcutMenu;
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
    // Fase 7: idioma ("es" por defecto; solo "es"|"en" son válidos).
    QString language() const;
    // Fase 7: apariencia ("light" por defecto) y contraste reforzado (off).
    QString theme() const;
    bool highContrast() const;
    // Fase 7: densidad táctil del POS ("normal" = 48px; ver Theme.posTouchH).
    QString posDensity() const;
    // Fase 7: atajo para una acción (clave shortcut_*; default si falta).
    Q_INVOKABLE QString shortcut(const QString &key) const;
    // Defaults de atajos (misma tabla que valida save()).
    static QVariantMap defaultShortcuts();
    static const QStringList ShortcutKeys;
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
