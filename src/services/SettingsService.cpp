#include "SettingsService.h"

#include "../core/EventBus.h"
#include "../repositories/SettingsRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeySequence>
#include <QSet>

const QString SettingsService::KBusinessName = QStringLiteral("business_name");
const QString SettingsService::KBusinessType = QStringLiteral("business_type");
const QString SettingsService::KNit = QStringLiteral("business_nit");
const QString SettingsService::KAddress = QStringLiteral("business_address");
const QString SettingsService::KPhone = QStringLiteral("business_phone");
const QString SettingsService::KLogoPath = QStringLiteral("business_logo_path");
const QString SettingsService::KCurrencySymbol = QStringLiteral("currency_symbol");
const QString SettingsService::KCurrencyDecimals = QStringLiteral("currency_decimals");
const QString SettingsService::KDefaultTaxRate = QStringLiteral("default_tax_rate");
const QString SettingsService::KTaxRatesJson = QStringLiteral("tax_rates_json");
const QString SettingsService::KMoraRate = QStringLiteral("mora_rate_monthly");
const QString SettingsService::KRequireExpiry = QStringLiteral("require_expiry");
const QString SettingsService::KRequireSerial = QStringLiteral("require_serial");
const QString SettingsService::KWeightUnit = QStringLiteral("weight_unit_default");
const QString SettingsService::KLanguage = QStringLiteral("language");
const QString SettingsService::KTheme = QStringLiteral("theme");
const QString SettingsService::KHighContrast = QStringLiteral("high_contrast");
const QString SettingsService::KPosDensity = QStringLiteral("pos_density");
const QString SettingsService::KShortcutDashboard = QStringLiteral("shortcut_dashboard");
const QString SettingsService::KShortcutPos = QStringLiteral("shortcut_pos");
const QString SettingsService::KShortcutProducts = QStringLiteral("shortcut_products");
const QString SettingsService::KShortcutSales = QStringLiteral("shortcut_sales");
const QString SettingsService::KShortcutInventory = QStringLiteral("shortcut_inventory");
const QString SettingsService::KShortcutReports = QStringLiteral("shortcut_reports");
const QString SettingsService::KShortcutMenu = QStringLiteral("shortcut_menu");

const QStringList SettingsService::ShortcutKeys = {
    KShortcutDashboard, KShortcutPos,      KShortcutProducts, KShortcutSales,
    KShortcutInventory, KShortcutReports,  KShortcutMenu,
};
const QString SettingsService::KCreditDays = QStringLiteral("credit_days");
const QString SettingsService::KPayableDays = QStringLiteral("payable_days");
const QString SettingsService::KDefaultCreditLimit = QStringLiteral("default_credit_limit");
const QString SettingsService::KFixedCostsMonthly = QStringLiteral("fixed_costs_monthly");
const QString SettingsService::KPromoVolumenMinQty = QStringLiteral("promo_volumen_min_qty");
const QString SettingsService::KFolioSeriesJson = QStringLiteral("folio_series_json");

const QStringList SettingsService::BusinessTypes = {
    QStringLiteral("farmacia"),    QStringLiteral("abarrotes"),   QStringLiteral("celulares"),
    QStringLiteral("miscelanea"),  QStringLiteral("ferreteria"),  QStringLiteral("ropa"),
    QStringLiteral("restaurante"), QStringLiteral("cafeteria"),   QStringLiteral("panaderia"),
    QStringLiteral("peluqueria"),  QStringLiteral("taller"),      QStringLiteral("lavanderia"),
    QStringLiteral("consultorio"), QStringLiteral("veterinaria"),
};

SettingsService::SettingsService(SettingsRepository *repo, EventBus *bus, QObject *parent)
    : QObject(parent), m_repo(repo), m_bus(bus)
{
}

QString SettingsService::businessName() const
{
    return m_repo->get(KBusinessName, QStringLiteral("Mi Negocio"));
}
QString SettingsService::businessType() const
{
    return m_repo->get(KBusinessType, QStringLiteral("miscelanea"));
}
QString SettingsService::nit() const
{
    return m_repo->get(KNit);
}
QString SettingsService::address() const
{
    return m_repo->get(KAddress);
}
QString SettingsService::phone() const
{
    return m_repo->get(KPhone);
}
QString SettingsService::logoPath() const
{
    return m_repo->get(KLogoPath);
}
QString SettingsService::currencySymbol() const
{
    return m_repo->get(KCurrencySymbol, QStringLiteral("$"));
}
int SettingsService::currencyDecimals() const
{
    return m_repo->get(KCurrencyDecimals, QStringLiteral("0")).toInt();
}
QString SettingsService::defaultTaxRate() const
{
    return m_repo->get(KDefaultTaxRate, QStringLiteral("19"));
}
QString SettingsService::taxRatesJson() const
{
    return m_repo->get(
        KTaxRatesJson,
        QStringLiteral("[{\"name\":\"IVA 19%\",\"rate\":19},{\"name\":\"Excluido\",\"rate\":0}]"));
}
double SettingsService::moraRate() const
{
    return m_repo->get(KMoraRate, QStringLiteral("2")).toDouble();
}
bool SettingsService::requireExpiry() const
{
    return m_repo->get(KRequireExpiry, QStringLiteral("0")).toInt() != 0;
}
bool SettingsService::requireSerial() const
{
    return m_repo->get(KRequireSerial, QStringLiteral("0")).toInt() != 0;
}
QString SettingsService::weightUnit() const
{
    return m_repo->get(KWeightUnit, QStringLiteral("unidad"));
}
QString SettingsService::language() const
{
    const QString v = m_repo->get(KLanguage, QStringLiteral("es")).trimmed().toLower();
    return (v == QStringLiteral("en")) ? QStringLiteral("en") : QStringLiteral("es");
}
QString SettingsService::theme() const
{
    const QString v = m_repo->get(KTheme, QStringLiteral("light")).trimmed().toLower();
    return (v == QStringLiteral("dark")) ? QStringLiteral("dark") : QStringLiteral("light");
}
bool SettingsService::highContrast() const
{
    return m_repo->get(KHighContrast, QStringLiteral("0")).trimmed()
           == QStringLiteral("1");
}
QString SettingsService::posDensity() const
{
    const QString v = m_repo->get(KPosDensity, QStringLiteral("normal")).trimmed().toLower();
    if (v == QStringLiteral("compacto") || v == QStringLiteral("amplio"))
        return v;
    return QStringLiteral("normal");
}
QVariantMap SettingsService::defaultShortcuts()
{
    return {
        {KShortcutDashboard, QStringLiteral("Ctrl+1")},
        {KShortcutPos, QStringLiteral("Ctrl+2")},
        {KShortcutProducts, QStringLiteral("Ctrl+3")},
        {KShortcutSales, QStringLiteral("Ctrl+4")},
        {KShortcutInventory, QStringLiteral("Ctrl+5")},
        {KShortcutReports, QStringLiteral("Ctrl+6")},
        {KShortcutMenu, QStringLiteral("Ctrl+M")},
    };
}
QString SettingsService::shortcut(const QString &key) const
{
    const QVariantMap dflt = defaultShortcuts();
    if (!dflt.contains(key))
        return {};
    return m_repo->get(key, dflt[key].toString());
}

int SettingsService::creditDays() const
{
    bool ok = false;
    const int v = m_repo->get(KCreditDays, QStringLiteral("15")).toInt(&ok);
    return ok ? qBound(0, v, 365) : 15;
}
int SettingsService::payableDays() const
{
    bool ok = false;
    const int v = m_repo->get(KPayableDays, QStringLiteral("30")).toInt(&ok);
    return ok ? qBound(0, v, 365) : 30;
}
Money SettingsService::defaultCreditLimit() const
{
    bool ok = false;
    const double v = m_repo->get(KDefaultCreditLimit, QStringLiteral("5000000")).toDouble(&ok);
    return ok && v >= 0.0 ? Money::fromCop(v) : Money::fromCop(5000000.0);
}
Money SettingsService::fixedCostsMonthly() const
{
    bool ok = false;
    const double v = m_repo->get(KFixedCostsMonthly, QStringLiteral("5000000")).toDouble(&ok);
    return ok && v >= 0.0 ? Money::fromCop(v) : Money::fromCop(5000000.0);
}
int SettingsService::promoVolumenMinQty() const
{
    bool ok = false;
    const int v = m_repo->get(KPromoVolumenMinQty, QStringLiteral("10")).toInt(&ok);
    return ok ? qBound(1, v, 1000) : 10;
}
QString SettingsService::folioSeriesJson() const
{
    return m_repo->get(KFolioSeriesJson, defaultFolioSeriesJson());
}

QMap<QString, SettingsService::FolioSerie> SettingsService::defaultFolioSeries()
{
    return {
        {QStringLiteral("Cotización"), {QStringLiteral("COT"), QStringLiteral("QUOTE_COUNTER")}},
        {QStringLiteral("Pedido"), {QStringLiteral("PED"), QStringLiteral("ORDER_COUNTER")}},
        {QStringLiteral("Remisión"), {QStringLiteral("REM"), QStringLiteral("REM_COUNTER")}},
        {QStringLiteral("Factura"), {QStringLiteral("FE"), QStringLiteral("INVOICE_COUNTER")}},
        {QStringLiteral("Nota crédito"),
         {QStringLiteral("NC"), QStringLiteral("CREDIT_NOTE_COUNTER")}},
        {QStringLiteral("Nota cargo"), {QStringLiteral("ND"), QStringLiteral("DEBIT_NOTE_COUNTER")}},
    };
}

QString SettingsService::defaultFolioSeriesJson()
{
    // Canónico (mismas series hardcodeadas hoy en SaleRepository::createDocument).
    return QStringLiteral(
        "{\"Cotizaci\u00f3n\":{\"prefix\":\"COT\",\"counter\":\"QUOTE_COUNTER\"},"
        "\"Pedido\":{\"prefix\":\"PED\",\"counter\":\"ORDER_COUNTER\"},"
        "\"Remisi\u00f3n\":{\"prefix\":\"REM\",\"counter\":\"REM_COUNTER\"},"
        "\"Factura\":{\"prefix\":\"FE\",\"counter\":\"INVOICE_COUNTER\"},"
        "\"Nota cr\u00e9dito\":{\"prefix\":\"NC\",\"counter\":\"CREDIT_NOTE_COUNTER\"},"
        "\"Nota cargo\":{\"prefix\":\"ND\",\"counter\":\"DEBIT_NOTE_COUNTER\"}}");
}

QMap<QString, SettingsService::FolioSerie> SettingsService::folioSeries() const
{
    const QMap<QString, FolioSerie> dflt = defaultFolioSeries();
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(folioSeriesJson().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty())
        return dflt;
    QMap<QString, FolioSerie> out = dflt;
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!dflt.contains(it.key()) || !it.value().isObject())
            continue;
        const QJsonObject e = it.value().toObject();
        const QString prefix = e.value(QStringLiteral("prefix")).toString().trimmed();
        const QString counter = e.value(QStringLiteral("counter")).toString().trimmed();
        if (!prefix.isEmpty() && !counter.isEmpty())
            out[it.key()] = FolioSerie{prefix, counter};
    }
    return out;
}

QList<SettingsService::TaxRate> SettingsService::taxRates() const
{
    static const QList<TaxRate> kFallback = {
        {QStringLiteral("IVA 19%"), 19.0},
        {QStringLiteral("Excluido"), 0.0},
    };
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(taxRatesJson().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty())
        return kFallback;
    QList<TaxRate> out;
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject())
            continue;
        const auto o = v.toObject();
        bool ok = false;
        const double rate = o.value(QStringLiteral("rate")).toVariant().toDouble(&ok);
        const QString name = o.value(QStringLiteral("name")).toString().trimmed();
        if (!ok || name.isEmpty())
            continue;
        out << TaxRate{name, rate};
    }
    return out.isEmpty() ? kFallback : out;
}

double SettingsService::defaultTaxRateValue() const
{
    bool ok = false;
    const double v = defaultTaxRate().toDouble(&ok);
    return ok ? v : 19.0;
}

QString SettingsService::taxNameForRate(double rate) const
{
    for (const TaxRate &t : taxRates()) {
        if (qFuzzyCompare(t.rate + 1.0, rate + 1.0))
            return t.name;
    }
    return QStringLiteral("%1%").arg(rate);
}

QVariantMap SettingsService::all() const
{
    QVariantMap m = m_repo->getAll();
    // Defaults para BDs legadas que aún no tienen las claves.
    if (!m.contains(KBusinessName))
        m[KBusinessName] = QStringLiteral("Mi Negocio");
    if (!m.contains(KBusinessType))
        m[KBusinessType] = QStringLiteral("miscelanea");
    if (!m.contains(KCurrencySymbol))
        m[KCurrencySymbol] = QStringLiteral("$");
    if (!m.contains(KCurrencyDecimals))
        m[KCurrencyDecimals] = QStringLiteral("0");
    if (!m.contains(KDefaultTaxRate))
        m[KDefaultTaxRate] = QStringLiteral("19");
    if (!m.contains(KTaxRatesJson))
        m[KTaxRatesJson] = QStringLiteral(
            "[{\"name\":\"IVA 19%\",\"rate\":19},{\"name\":\"Excluido\",\"rate\":0}]");
    if (!m.contains(KMoraRate))
        m[KMoraRate] = QStringLiteral("2");
    if (!m.contains(KRequireExpiry))
        m[KRequireExpiry] = QStringLiteral("0");
    if (!m.contains(KRequireSerial))
        m[KRequireSerial] = QStringLiteral("0");
    if (!m.contains(KWeightUnit))
        m[KWeightUnit] = QStringLiteral("unidad");
    if (!m.contains(KLanguage))
        m[KLanguage] = QStringLiteral("es");
    if (!m.contains(KTheme))
        m[KTheme] = QStringLiteral("light");
    if (!m.contains(KHighContrast))
        m[KHighContrast] = QStringLiteral("0");
    if (!m.contains(KPosDensity))
        m[KPosDensity] = QStringLiteral("normal");
    const QVariantMap shortcutDefaults = defaultShortcuts();
    for (auto it = shortcutDefaults.begin(); it != shortcutDefaults.end(); ++it) {
        if (!m.contains(it.key()))
            m[it.key()] = it.value();
    }
    if (!m.contains(KCreditDays))
        m[KCreditDays] = QStringLiteral("15");
    if (!m.contains(KPayableDays))
        m[KPayableDays] = QStringLiteral("30");
    if (!m.contains(KDefaultCreditLimit))
        m[KDefaultCreditLimit] = QStringLiteral("5000000");
    if (!m.contains(KFixedCostsMonthly))
        m[KFixedCostsMonthly] = QStringLiteral("5000000");
    if (!m.contains(KPromoVolumenMinQty))
        m[KPromoVolumenMinQty] = QStringLiteral("10");
    if (!m.contains(KFolioSeriesJson))
        m[KFolioSeriesJson] = defaultFolioSeriesJson();
    return m;
}

QString SettingsService::validate(const QVariantMap &m, QVariantMap &cleaned) const
{
    cleaned.clear();
    static const QStringList kKnown = {
        KBusinessName,
        KBusinessType,
        KNit,
        KAddress,
        KPhone,
        KLogoPath,
        KCurrencySymbol,
        KCurrencyDecimals,
        KDefaultTaxRate,
        KTaxRatesJson,
        KMoraRate,
        KRequireExpiry,
        KRequireSerial,
        KWeightUnit,
        KLanguage,
        KTheme,
        KHighContrast,
        KPosDensity,
        KShortcutDashboard,
        KShortcutPos,
        KShortcutProducts,
        KShortcutSales,
        KShortcutInventory,
        KShortcutReports,
        KShortcutMenu,
        KCreditDays,
        KPayableDays,
        KDefaultCreditLimit,
        KFixedCostsMonthly,
        KPromoVolumenMinQty,
        KFolioSeriesJson,
        QStringLiteral("business_tax_id"),
    };
    for (auto it = m.begin(); it != m.end(); ++it) {
        if (!kKnown.contains(it.key()))
            continue; // ignorar claves desconocidas (compatibilidad)
        cleaned[it.key()] = it.value().toString().trimmed();
    }
    if (cleaned.contains(KBusinessName) && cleaned[KBusinessName].toString().isEmpty())
        return QStringLiteral("El nombre del negocio no puede estar vacío");
    if (cleaned.contains(KBusinessType)
        && !BusinessTypes.contains(cleaned[KBusinessType].toString())
        && cleaned[KBusinessType].toString() != QStringLiteral("miscelanea"))
        return QStringLiteral("Tipo de negocio inválido: %1")
            .arg(cleaned[KBusinessType].toString());
    if (cleaned.contains(KCurrencyDecimals)) {
        bool ok = false;
        const int d = cleaned[KCurrencyDecimals].toInt(&ok);
        if (!ok || d < 0 || d > 2)
            return QStringLiteral("Decimales de moneda inválidos (0-2)");
        cleaned[KCurrencyDecimals] = QString::number(d);
    }
    if (cleaned.contains(KTaxRatesJson)) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(cleaned[KTaxRatesJson].toByteArray(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray())
            return QStringLiteral("Lista de tasas inválida (JSON array esperado)");
    }
    if (cleaned.contains(KMoraRate)) {
        bool ok = false;
        const double v = cleaned[KMoraRate].toDouble(&ok);
        if (!ok || v < 0 || v > 100)
            return QStringLiteral("Tasa de mora inválida (0-100)");
    }
    for (const QString &flag : {KRequireExpiry, KRequireSerial}) {
        if (cleaned.contains(flag)) {
            const QString v = cleaned[flag].toString();
            if (v != QStringLiteral("0") && v != QStringLiteral("1"))
                return QStringLiteral("Flag inválido para %1 (0/1)").arg(flag);
        }
    }
    if (cleaned.contains(KLanguage)) {
        const QString v = cleaned[KLanguage].toString().trimmed().toLower();
        if (v != QStringLiteral("es") && v != QStringLiteral("en"))
            return QStringLiteral("Idioma inválido para %1 (es/en)").arg(KLanguage);
        cleaned[KLanguage] = v;
    }
    if (cleaned.contains(KTheme)) {
        const QString v = cleaned[KTheme].toString().trimmed().toLower();
        if (v != QStringLiteral("light") && v != QStringLiteral("dark"))
            return QStringLiteral("Tema inválido para %1 (light/dark)").arg(KTheme);
        cleaned[KTheme] = v;
    }
    if (cleaned.contains(KHighContrast)) {
        const QString v = cleaned[KHighContrast].toString().trimmed();
        if (v != QStringLiteral("0") && v != QStringLiteral("1"))
            return QStringLiteral("Flag inválido para %1 (0/1)").arg(KHighContrast);
        cleaned[KHighContrast] = v;
    }
    if (cleaned.contains(KPosDensity)) {
        const QString v = cleaned[KPosDensity].toString().trimmed().toLower();
        if (v != QStringLiteral("compacto") && v != QStringLiteral("normal")
            && v != QStringLiteral("amplio"))
            return QStringLiteral("Densidad inválida para %1 (compacto/normal/amplio)")
                .arg(KPosDensity);
        cleaned[KPosDensity] = v;
    }
    // Atajos: secuencia válida de QKeySequence, no vacía y sin duplicados
    // (dos acciones con la misma secuencia dispararían ambas a la vez).
    // Se exige forma Portable (p. ej. "Ctrl+Shift+P"): así se rechazan
    // teclas desconocidas ("NotAKey") o modificadores solos ("Ctrl"), que
    // QKeySequence acepta pero nunca dispararían.
    {
        QMap<QString, QString> effective;
        const QVariantMap dflt = defaultShortcuts();
        for (const QString &key : ShortcutKeys)
            effective[key] = m_repo->get(key, dflt[key].toString());
        for (const QString &key : ShortcutKeys) {
            if (cleaned.contains(key))
                effective[key] = cleaned[key].toString().trimmed();
        }
        auto portableOf = [](const QString &seq, QString *out) -> bool {
            const QKeySequence ks(seq);
            if (ks.count() == 0)
                return false;
            for (int i = 0; i < ks.count(); ++i) {
                if (ks[i].key() == Qt::Key_unknown)
                    return false;
            }
            const QString norm = ks.toString(QKeySequence::PortableText);
            if (norm.isEmpty())
                return false;
            if (out)
                *out = norm;
            return true;
        };
        QSet<QString> seen;
        for (const QString &key : ShortcutKeys) {
            const QString seq = effective[key].trimmed();
            if (seq.isEmpty())
                return QStringLiteral("Atajo vacío para %1").arg(key);
            QString norm;
            if (!portableOf(seq, &norm))
                return QStringLiteral("Atajo inválido para %1 (%2)").arg(key, seq);
            if (seen.contains(norm))
                return QStringLiteral("Atajo duplicado: %1 ya está asignado").arg(seq);
            seen.insert(norm);
        }
        for (const QString &key : ShortcutKeys) {
            if (cleaned.contains(key))
                cleaned[key] = effective[key].trimmed();
        }
    }
    for (const QString &key : {KCreditDays, KPayableDays}) {
        if (cleaned.contains(key)) {
            bool ok = false;
            const int v = cleaned[key].toInt(&ok);
            if (!ok || v < 0 || v > 365)
                return QStringLiteral("Días inválidos para %1 (0-365)").arg(key);
            cleaned[key] = QString::number(v);
        }
    }
    if (cleaned.contains(KPromoVolumenMinQty)) {
        bool ok = false;
        const int v = cleaned[KPromoVolumenMinQty].toInt(&ok);
        if (!ok || v < 1 || v > 1000)
            return QStringLiteral("Cantidad mínima de promo inválida (1-1000)");
        cleaned[KPromoVolumenMinQty] = QString::number(v);
    }
    for (const QString &key : {KDefaultCreditLimit, KFixedCostsMonthly}) {
        if (cleaned.contains(key)) {
            bool ok = false;
            const double v = cleaned[key].toDouble(&ok);
            if (!ok || v < 0.0)
                return QStringLiteral("Importe inválido para %1 (>= 0)").arg(key);
        }
    }
    if (cleaned.contains(KFolioSeriesJson)) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(cleaned[KFolioSeriesJson].toByteArray(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty())
            return QStringLiteral("Series de folios inválidas (objeto JSON esperado)");
        // Nota: iterar sobre un QJsonObject nombrado (begin()/end() de
        // temporales distintos es UB: compara punteros y el bucle lee
        // basura — el save de folios fallaba siempre).
        const QJsonObject seriesObj = doc.object();
        for (auto it = seriesObj.begin(); it != seriesObj.end(); ++it) {
            if (!it.value().isObject())
                return QStringLiteral("Series de folios inválidas (%1 sin prefijo/contador)")
                    .arg(it.key());
            const QJsonObject e = it.value().toObject();
            if (e.value(QStringLiteral("prefix")).toString().trimmed().isEmpty()
                || e.value(QStringLiteral("counter")).toString().trimmed().isEmpty())
                return QStringLiteral("Series de folios inválidas (%1 sin prefijo/contador)")
                    .arg(it.key());
        }
    }
    return {};
}

QVariantMap SettingsService::save(const QVariantMap &m)
{
    QVariantMap cleaned;
    if (const QString err = validate(m, cleaned); !err.isEmpty())
        return {{"ok", false}, {"error", err}};
    if (cleaned.isEmpty())
        return {{"ok", true}};
    if (!m_repo->setAll(cleaned))
        return {{"ok", false}, {"error", QStringLiteral("No se pudo guardar la configuración")}};
    emit settingsChanged();
    if (m_bus)
        m_bus->publish(EventBus::SettingsChanged, cleaned);
    return {{"ok", true}};
}
