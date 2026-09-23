#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/SaleRepository.h"
#include "../repositories/SerialRepository.h"
#include "../services/SalesService.h"

// Historial, documentos y notas (antes SalesScreen).
class SalesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sales READ sales NOTIFY salesChanged)

public:
    explicit SalesController(SaleRepository *sales, SalesService *service,
                             SerialRepository *serials = nullptr,
                             ProductRepository *products = nullptr, QObject *parent = nullptr);

    QVariantList sales() const { return m_sales; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap detail(const QString &id) const;
    Q_INVOKABLE QVariantMap advance(const QString &id, const QString &status,
                                    const QString &user);
    Q_INVOKABLE QVariantMap cancel(const QString &id, const QString &reason,
                                   const QString &user);
    Q_INVOKABLE QVariantMap createDoc(const QString &type, const QString &client,
                                      double total, const QString &user);
    Q_INVOKABLE QVariantMap creditNote(const QString &id, double amount,
                                       const QString &reason, const QString &user);
    Q_INVOKABLE QVariantMap debitNote(const QString &id, double amount,
                                      const QString &reason, const QString &user);
    // Fase 3: garantía por serial + RMA.
    Q_INVOKABLE QVariantMap warrantyFor(const QString &serial) const;
    Q_INVOKABLE QVariantMap markRma(const QString &serial, const QString &notes,
                                    const QString &user);

    static QVariantMap toMap(const Sale &s);

signals:
    void salesChanged();

private:
    SaleRepository *m_repos = nullptr;
    SalesService *m_service = nullptr;
    SerialRepository *m_serials = nullptr;
    ProductRepository *m_products = nullptr;
    QVariantList m_sales;
};
