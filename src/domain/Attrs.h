#pragma once

// Fase 3: metadatos por vertical sobre columnas attrs_json (products, sale_items).
// Claves canónicas: track_serial (bool), warranty_months (int),
// requires_prescription (bool), controlled (bool). Sin Q_OBJECT: helpers puros.
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>

namespace Attrs
{
inline QJsonObject parse(const QString &json)
{
    if (json.trimmed().isEmpty())
        return {};
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    return (err.error == QJsonParseError::NoError && doc.isObject()) ? doc.object() : QJsonObject{};
}

inline QString dump(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

inline bool boolean(const QString &json, const QString &key)
{
    return parse(json).value(key).toBool(false);
}

inline int integer(const QString &json, const QString &key, int fallback = 0)
{
    const QJsonValue v = parse(json).value(key);
    return v.isDouble() ? v.toInt(fallback) : fallback;
}

inline QString set(const QString &json, const QString &key, const QVariant &value)
{
    QJsonObject o = parse(json);
    o[key] = QJsonValue::fromVariant(value);
    return dump(o);
}

inline bool isObject(const QString &json)
{
    if (json.trimmed().isEmpty())
        return true; // vacío = objeto vacío implícito
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}

// Claves canónicas
inline const QString KTrackSerial = QStringLiteral("track_serial");
inline const QString KWarrantyMonths = QStringLiteral("warranty_months");
inline const QString KRequiresPrescription = QStringLiteral("requires_prescription");
inline const QString KControlled = QStringLiteral("controlled");
inline const QString KReceta = QStringLiteral("receta");
} // namespace Attrs
