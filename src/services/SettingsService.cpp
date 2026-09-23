#include "SettingsService.h"

#include "../core/EventBus.h"
#include "../repositories/SettingsRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

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

const QStringList SettingsService::BusinessTypes = {
    QStringLiteral("farmacia"),      QStringLiteral("abarrotes"),
    QStringLiteral("celulares"),     QStringLiteral("miscelanea"),
    QStringLiteral("ferreteria"),    QStringLiteral("ropa"),
    QStringLiteral("restaurante"),   QStringLiteral("cafeteria"),
    QStringLiteral("panaderia"),     QStringLiteral("peluqueria"),
    QStringLiteral("taller"),        QStringLiteral("lavanderia"),
    QStringLiteral("consultorio"),   QStringLiteral("veterinaria"),
};

namespace
{
QString withDefault(const QVariantMap &all, const QString &key, const QString &dflt)
{
    return all.value(key, dflt).toString();
}
} // namespace

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
        m[KTaxRatesJson] =
            QStringLiteral("[{\"name\":\"IVA 19%\",\"rate\":19},{\"name\":\"Excluido\",\"rate\":0}]");
    if (!m.contains(KMoraRate))
        m[KMoraRate] = QStringLiteral("2");
    if (!m.contains(KRequireExpiry))
        m[KRequireExpiry] = QStringLiteral("0");
    if (!m.contains(KRequireSerial))
        m[KRequireSerial] = QStringLiteral("0");
    if (!m.contains(KWeightUnit))
        m[KWeightUnit] = QStringLiteral("unidad");
    return m;
}

QString SettingsService::validate(const QVariantMap &m, QVariantMap &cleaned) const
{
    cleaned.clear();
    static const QStringList kKnown = {
        KBusinessName, KBusinessType, KNit, KAddress, KPhone, KLogoPath,
        KCurrencySymbol, KCurrencyDecimals, KDefaultTaxRate, KTaxRatesJson,
        KMoraRate, KRequireExpiry, KRequireSerial, KWeightUnit,
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
        const auto doc =
            QJsonDocument::fromJson(cleaned[KTaxRatesJson].toByteArray(), &err);
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
