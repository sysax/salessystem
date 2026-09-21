#pragma once

// Facturación DIAN — subset mínimo para fase 3 (lo que usa SaleRepository).
// Modo OFF por defecto (documentos internos). Fase 4: servicio completo
// (validación NIT, UBL, envío). Port de data/dian.py.
#include <QSqlDatabase>
#include <QString>

namespace Dian
{
bool isEnabled(QSqlDatabase db);
QString provider(QSqlDatabase db);
QString defaultDocType(QSqlDatabase db); // "Ticket de venta" si OFF
QString generateCufe(QSqlDatabase db, const QString &folio);
} // namespace Dian
