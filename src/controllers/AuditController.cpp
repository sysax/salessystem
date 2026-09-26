#include "AuditController.h"

AuditController::AuditController(AuditRepository *audit, QObject *parent)
    : QObject(parent), m_repos(audit)
{
    search({});
}

QVariantMap AuditController::toMap(const AuditEntry &e)
{
    return {{"id", e.id},
            {"ts", e.ts},
            {"user", e.user},
            {"action", e.action},
            {"detail", e.detail},
            {"entity", e.entity},
            {"entityId", e.entityId},
            {"before", e.beforeJson},
            {"after", e.afterJson}};
}

void AuditController::search(const QString &text, const QString &user, const QString &action)
{
    m_entries.clear();
    if (!m_repos) {
        emit entriesChanged();
        return;
    }
    for (const AuditEntry &e : m_repos->search(text, user, action, 200))
        m_entries << toMap(e);
    emit entriesChanged();
}
