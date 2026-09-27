#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "../repositories/CreditRepository.h"

class EventBus;

// Fachadas delgadas sobre los repos de crédito: validan contexto de caja
// (usuario) y publican eventos. La lógica de saldos vive en los repos.
class ReceivablesService : public QObject
{
    Q_OBJECT

  public:
    explicit ReceivablesService(ReceivablesRepository *repos, EventBus *bus = nullptr,
                                QObject *parent = nullptr);

    Q_INVOKABLE QVariantList pending() const;
    Q_INVOKABLE QVariantList statement(const QString &client) const;
    Result<Sale> pay(const QString &saleId, Money amount, const QString &method,
                     const QString &user);
    static Money mora(const Sale &s);

  private:
    ReceivablesRepository *m_repos = nullptr;
    EventBus *m_bus = nullptr;
};

class PayablesService : public QObject
{
    Q_OBJECT

  public:
    explicit PayablesService(PayablesRepository *repos, EventBus *bus = nullptr,
                             QObject *parent = nullptr);

    Q_INVOKABLE QVariantList pending() const;
    // Fase 5: vencidas, estado por proveedor e historial de abonos.
    Q_INVOKABLE QVariantList overdue() const;
    Q_INVOKABLE QVariantList statement(const QString &supplier) const;
    Result<PayablesRepository::PaymentResult> pay(const QString &payableId, Money amount,
                                                  const QString &method, const QString &user);

  private:
    PayablesRepository *m_repos = nullptr;
    EventBus *m_bus = nullptr;
};
