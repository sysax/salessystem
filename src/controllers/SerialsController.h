#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../domain/Attrs.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/SerialRepository.h"

// Fase 4: gestión de seriales/IMEI para QML (antes solo vía POS).
// Registro, listado con filtro, garantía y RMA.
class SerialsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList serials READ serials NOTIFY serialsChanged)

public:
    explicit SerialsController(SerialRepository *serials, ProductRepository *products,
                               QObject *parent = nullptr);

    QVariantList serials() const { return m_serials; }

    Q_INVOKABLE void search(const QString &text, const QString &status = {});
    Q_INVOKABLE QVariantMap addSerial(const QString &sku, const QString &serial,
                                      const QString &imei2 = {});
    Q_INVOKABLE QVariantMap setStatus(const QString &serial, const QString &status,
                                      const QString &notes, const QString &user);
    Q_INVOKABLE QVariantMap warrantyFor(const QString &serial) const;
    Q_INVOKABLE QVariantList inStock(const QString &sku) const;
    Q_INVOKABLE int inStockCount(const QString &sku) const;

    static QVariantMap toMap(const SerialInfo &s, const QString &productName = {});

signals:
    void serialsChanged();

private:
    SerialRepository *m_repos = nullptr;
    ProductRepository *m_products = nullptr;
    QVariantList m_serials;
};
