#pragma once

#include <QSqlDatabase>
#include <QString>

// Fase 1 (MAP_PRO.md): transacción RAII. Si se destruye sin commit(),
// revierte automáticamente. Soporta anidamiento vía SAVEPOINTs: un
// servicio puede abrir una transacción externa y los repositorios
// abrir la suya propia sin romper la atomicidad (el commit externo
// decide; un fallo interno revierte todo).
class Transaction
{
  public:
    explicit Transaction(QSqlDatabase &db);
    ~Transaction();

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    // Confirma (o libera el savepoint si es anidada). Falso si el
    // commit falló (en ese caso ya se intentó revertir).
    bool commit();
    // Revierte explícitamente (el destructor lo hace solo si falta).
    void rollback();
    bool isValid() const
    {
        return m_active;
    }

  private:
    QSqlDatabase *m_db = nullptr;
    QString m_savepoint;
    bool m_active = false;
    bool m_committed = false;
};
