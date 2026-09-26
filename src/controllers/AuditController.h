#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/AuditRepository.h"

// Fase 5: explorador de bitácora para administración (solo rol
// Administrador vía sidebar). Responde "¿quién cambió este precio?".
class AuditController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

  public:
    explicit AuditController(AuditRepository *audit, QObject *parent = nullptr);

    QVariantList entries() const
    {
        return m_entries;
    }

    Q_INVOKABLE void search(const QString &text = {}, const QString &user = {},
                            const QString &action = {});

    static QVariantMap toMap(const AuditEntry &e);

  signals:
    void entriesChanged();

  private:
    AuditRepository *m_repos = nullptr;
    QVariantList m_entries;
};
