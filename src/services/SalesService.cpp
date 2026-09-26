#include "SalesService.h"

#include "../core/EventBus.h"
#include "../core/Money.h"
#include "../domain/Attrs.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

SalesService::SalesService(QSqlDatabase db, ProductRepository *products, SaleRepository *sales,
                           InventoryRepository *inventory, ClientRepository *clients,
                           CajaRepository *caja, PromoRepository *promos, EventBus *bus,
                           SettingsService *settings, AuditRepository *audit,
                           SerialRepository *serials, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_sales(sales),
      m_inventory(inventory), m_clients(clients), m_caja(caja), m_promos(promos), m_bus(bus),
      m_settings(settings), m_audit(audit), m_serials(serials)
{
}

bool SalesService::isTracked(const Product &p) const
{
    if (Attrs::boolean(p.attrsJson, Attrs::KTrackSerial))
        return true;
    return m_serials && m_serials->hasSerials(p.sku);
}

double SalesService::parseTaxRate(const QString &taxText)
{
    const QString t = taxText.trimmed();
    if (t.isEmpty())
        return 0.0;
    // "IVA 19%" / "19 %" / "19" → 19 ; "Exento"/"0" → 0
    QString digits;
    bool dotSeen = false;
    bool started = false;
    for (const QChar c : t) {
        if (c.isDigit()) {
            digits += c;
            started = true;
        } else if ((c == u'.' || c == u',') && started && !dotSeen) {
            digits += u'.';
            dotSeen = true;
        } else if (started) {
            break;
        }
    }
    bool ok = false;
    const double v = digits.toDouble(&ok);
    return ok ? v : 0.0;
}

double SalesService::resolveTaxRate(const QString &taxText) const
{
    const double parsed = parseTaxRate(taxText);
    if (!m_settings)
        return parsed; // legacy: sin configuración no se valida
    for (const auto &t : m_settings->taxRates()) {
        if (qFuzzyCompare(t.rate + 1.0, parsed + 1.0))
            return t.rate;
    }
    // Tasa no configurada (p. ej. "IVA 8%" heredado) → tasa por defecto.
    // Evita el "IVA fantasma": nunca se liquida una tasa fuera de la lista.
    return m_settings->defaultTaxRateValue();
}

QString SalesService::resolveTaxName(const QString &taxText) const
{
    if (!m_settings)
        return taxText.trimmed();
    return m_settings->taxNameForRate(resolveTaxRate(taxText));
}

QString SalesService::bucketsToJson(const QList<TaxBucket> &buckets)
{
    QJsonArray arr;
    for (const TaxBucket &b : buckets) {
        arr << QJsonObject{{QStringLiteral("name"), b.name},
                           {QStringLiteral("rate"), b.rate},
                           {QStringLiteral("base"), b.base},
                           {QStringLiteral("tax"), b.tax}};
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QList<SalesService::TaxBucket> SalesService::bucketsFromJson(const QString &json)
{
    QList<TaxBucket> out;
    if (json.trimmed().isEmpty())
        return out;
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return out;
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject())
            continue;
        const auto o = v.toObject();
        TaxBucket b;
        b.name = o.value(QStringLiteral("name")).toString();
        b.rate = o.value(QStringLiteral("rate")).toDouble();
        b.base = o.value(QStringLiteral("base")).toDouble();
        b.tax = o.value(QStringLiteral("tax")).toDouble();
        out << b;
    }
    return out;
}

double SalesService::priceFor(const Product &p, double priceOverride,
                              const QString &clientName) const
{
    if (priceOverride > 0)
        return priceOverride;
    // Fase 4 (abarrotes): clientes mayoristas pagan price_wholesale.
    if (!clientName.trimmed().isEmpty() && m_clients) {
        const auto c = m_clients->findByName(clientName.trimmed());
        if (c) {
            const QString pl = c->priceList.trimmed().toLower();
            if ((pl == QLatin1String("mayorista") || pl == QLatin1String("mayoreo")
                 || pl == QLatin1String("wholesale"))
                && p.priceWholesale > 0)
                return p.priceWholesale;
        }
    }
    return p.price;
}

namespace
{
// Fase 2: impuesto de línea en céntimos exactos (round-half-up).
double lineTax(double base, double ratePct)
{
    return Money::taxCents(Money::fromCop(base).cents(), ratePct)
           / static_cast<double>(Money::CentsPerCop);
}
// Agrega una línea al bucket de su tasa (agregación, sin recalcular).
void accumulateBucket(QList<SalesService::TaxBucket> &buckets, const QString &name, double rate,
                      double lineBase, double lineTax)
{
    for (SalesService::TaxBucket &b : buckets) {
        if (qFuzzyCompare(b.rate + 1.0, rate + 1.0)) {
            b.base += lineBase;
            b.tax += lineTax;
            return;
        }
    }
    buckets << SalesService::TaxBucket{name, rate, lineBase, lineTax};
}
} // namespace

Result<SalesService::Totals> SalesService::buildTotals(const QList<ServiceItem> &items,
                                                       QString &error,
                                                       const QString &clientName) const
{
    Totals t;
    t.itemsCount = items.size();
    const QDate today = QDate::currentDate();
    QSet<QString> usedSerials;
    // Fase 4: en productos con serial, cada línea es 1 unidad con 1 serial.
    // Contar líneas tracked por producto para no vender más que seriales in_stock.
    QMap<int, int> trackedLines;
    for (const ServiceItem &it : items) {
        const auto p = m_products->findById(it.productId);
        if (p && isTracked(*p))
            trackedLines[it.productId] += 1;
    }
    if (m_serials) {
        for (auto it = trackedLines.begin(); it != trackedLines.end(); ++it) {
            const auto p = m_products->findById(it.key());
            if (p && m_serials->inStockCount(p->sku) < it.value()) {
                error = QStringLiteral(
                            "Sin seriales suficientes para '%1'. Disponibles: %2, solicitados: %3")
                            .arg(p->name)
                            .arg(m_serials->inStockCount(p->sku))
                            .arg(it.value());
                return Result<Totals>::failure(error);
            }
        }
    }
    for (const ServiceItem &it : items) {
        const auto p = m_products->findById(it.productId);
        if (!p) {
            error = QStringLiteral("Producto ID %1 no existe").arg(it.productId);
            return Result<Totals>::failure(error);
        }
        // Multitienda (filtrar sin borrar): el rubro activo no vende productos
        // etiquetados de otro rubro. '' = legacy/mixto (siempre permitido) y
        // 'miscelanea' = modo mixto intencional (permite todo).
        if (m_settings) {
            const QString bt = m_settings->businessType().trimmed();
            const QString pb = p->businessType.trimmed();
            if (!bt.isEmpty() && bt != QLatin1String("miscelanea") && !pb.isEmpty()
                && pb != QLatin1String("miscelanea") && pb != bt) {
                error = QStringLiteral("Producto '%1' es de '%2' y el rubro activo es '%3' "
                                       "(oculto, no se borra: cambie de rubro para venderlo)")
                            .arg(p->name, pb, bt);
                return Result<Totals>::failure(error);
            }
        }
        // Fase 3: se vende contra disponible (físico menos apartados).
        if (p->available() < it.qty - 1e-9) {
            error = QStringLiteral("Stock insuficiente para '%1'. Disponible: %2, Solicitado: %3")
                        .arg(p->name)
                        .arg(p->available())
                        .arg(it.qty);
            return Result<Totals>::failure(error);
        }
        // Fase 3: nunca vender vencidos (aplica siempre, sin flag).
        if (!p->vencimiento.trimmed().isEmpty()) {
            const QDate venc = QDate::fromString(p->vencimiento.trimmed(), Qt::ISODate);
            if (venc.isValid() && venc < today) {
                error = QStringLiteral("Producto vencido: '%1' (lote %2, venció %3)")
                            .arg(p->name, p->lote.isEmpty() ? QStringLiteral("—") : p->lote,
                                 p->vencimiento);
                return Result<Totals>::failure(error);
            }
        }
        // Fase 3: receta obligatoria.
        if (Attrs::boolean(p->attrsJson, Attrs::KRequiresPrescription)
            && it.receta.trimmed().isEmpty()) {
            error = QStringLiteral("Receta requerida para '%1'").arg(p->name);
            return Result<Totals>::failure(error);
        }
        // Fase 3: serial obligatorio en productos tracked.
        const bool tracked = isTracked(*p);
        const QString serial = it.serial.trimmed();
        if (tracked) {
            // Fase 4 (criterio): 1 línea = 1 unidad = 1 serial; vender qty 2
            // con 1 serial queda rechazado aquí.
            if (qAbs(it.qty - 1.0) > 1e-9) {
                error = QStringLiteral(
                            "Producto con serial '%1': venda 1 unidad por línea con su serial")
                            .arg(p->name);
                return Result<Totals>::failure(error);
            }
            if (serial.isEmpty()) {
                error = QStringLiteral("Serial requerido para '%1'").arg(p->name);
                return Result<Totals>::failure(error);
            }
            if (usedSerials.contains(serial)) {
                error = QStringLiteral("Serial %1 repetido en la venta").arg(serial);
                return Result<Totals>::failure(error);
            }
            usedSerials.insert(serial);
            if (m_serials) {
                const auto s = m_serials->find(serial);
                if (!s) {
                    error = QStringLiteral("Serial %1 no registrado").arg(serial);
                    return Result<Totals>::failure(error);
                }
                if (s->status != QLatin1String("in_stock")) {
                    error = QStringLiteral("Serial %1 no disponible (%2)").arg(serial, s->status);
                    return Result<Totals>::failure(error);
                }
            }
        }
        LineTotal l;
        l.productId = p->id;
        l.name = p->name;
        l.sku = p->sku;
        l.qty = it.qty;
        l.serial = serial;
        l.receta = it.receta.trimmed();
        // Fase 4: precio según lista del cliente (mayorista → mayoreo).
        l.unitPrice = priceFor(*p, it.priceOverride, clientName);
        l.subtotal = l.unitPrice * it.qty;
        l.discount = l.subtotal * it.discountPct / 100.0;
        l.taxRate = resolveTaxRate(p->tax);
        l.taxName = resolveTaxName(p->tax);
        l.tax = lineTax(l.subtotal - l.discount, l.taxRate);
        l.total = l.subtotal - l.discount + l.tax;
        t.lines << l;
        t.subtotal += l.subtotal;
        t.discount += l.discount;
        t.tax += l.tax;
        accumulateBucket(t.buckets, l.taxName, l.taxRate, l.subtotal - l.discount, l.tax);
    }
    t.total = t.subtotal - t.discount + t.tax;
    return Result<Totals>::success(t);
}

SalesService::Totals SalesService::calculateTotals(const QList<ServiceItem> &items) const
{
    QString error;
    const auto r = buildTotals(items, error);
    if (r.ok())
        return r.value();
    // Dry-run tolerante: líneas inválidas se omiten (como en Python)
    Totals t;
    for (const ServiceItem &it : items) {
        const auto p = m_products->findById(it.productId);
        if (!p)
            continue;
        LineTotal l;
        l.productId = p->id;
        l.qty = it.qty;
        l.serial = it.serial.trimmed();
        l.receta = it.receta.trimmed();
        l.unitPrice = it.priceOverride > 0 ? it.priceOverride : p->price;
        l.subtotal = l.unitPrice * it.qty;
        l.discount = l.subtotal * it.discountPct / 100.0;
        l.taxRate = resolveTaxRate(p->tax);
        l.taxName = resolveTaxName(p->tax);
        l.tax = lineTax(l.subtotal - l.discount, l.taxRate);
        l.total = l.subtotal - l.discount + l.tax;
        t.lines << l;
        t.subtotal += l.subtotal;
        t.discount += l.discount;
        t.tax += l.tax;
        accumulateBucket(t.buckets, l.taxName, l.taxRate, l.subtotal - l.discount, l.tax);
    }
    t.itemsCount = items.size();
    t.total = t.subtotal - t.discount + t.tax;
    return t;
}

Result<SalesService::CreatedSale>
SalesService::create(const QList<ServiceItem> &items, const QString &clientName,
                     const QMap<QString, double> &payments, const QString &paymentMethod,
                     const QString &promoCode, const QString &vendedor, bool offline,
                     const QString &role)
{
    if (items.isEmpty())
        return Result<CreatedSale>::failure(QStringLiteral("La venta debe tener al menos un item"));

    QString error;
    auto totals = buildTotals(items, error, clientName);
    if (!totals.ok())
        return Result<CreatedSale>::failure(totals.error());

    // Fase 3: productos controlados exigen rol supervisor (Administrador).
    if (role.trimmed() != QLatin1String("Administrador")) {
        for (const LineTotal &l : totals.value().lines) {
            const auto p = m_products->findById(l.productId);
            if (p && Attrs::boolean(p->attrsJson, Attrs::KControlled))
                return Result<CreatedSale>::failure(
                    QStringLiteral("'%1' es controlado: requiere supervisor").arg(p->name));
        }
    }

    // Promo (código vacío → sin descuento, sin error)
    double promoDiscount = 0.0;
    QString promoUsed;
    if (!promoCode.trimmed().isEmpty()) {
        QList<CartLine> cart;
        for (const LineTotal &l : totals.value().lines) {
            CartLine c;
            c.productId = l.productId;
            c.sku = l.sku;
            c.price = l.unitPrice;
            c.qty = l.qty;
            c.subtotal = l.subtotal;
            cart << c;
        }
        auto promo = m_promos->evaluate(cart, promoCode,
                                        m_settings ? m_settings->businessType() : QString());
        if (!promo.ok())
            return Result<CreatedSale>::failure(promo.error());
        promoDiscount = promo.value().discount;
        promoUsed = promo.value().promoCode;
    }
    const double total = totals.value().total - promoDiscount;

    // Fase 3: la venta a crédito no puede superar el límite del cliente
    // (saldo pendiente + nuevo crédito <= límite). Cliente desconocido
    // (Mostrador sin ficha) no se valida: no hay límite contra qué.
    if (m_clients) {
        const double creditPart = payments.value(QStringLiteral("credito"), 0.0);
        const bool toCredit = creditPart > 0
            || (payments.isEmpty() && paymentMethod.trimmed() == QLatin1String("Credito"));
        if (toCredit) {
            const double newCredit = payments.isEmpty() ? total : creditPart;
            if (const auto c = m_clients->findByName(clientName.trimmed())) {
                if (c->creditLimit <= 0) {
                    return Result<CreatedSale>::failure(
                        QStringLiteral("'%1' no tiene crédito asignado").arg(c->name));
                }
                if (c->balance + newCredit > c->creditLimit + 1e-9) {
                    return Result<CreatedSale>::failure(
                        QStringLiteral("Límite de crédito excedido para '%1' (saldo %2 + venta %3 > "
                                       "límite %4)")
                            .arg(c->name)
                            .arg(c->balance, 0, 'f', 0)
                            .arg(newCredit, 0, 'f', 0)
                            .arg(c->creditLimit, 0, 'f', 0));
                }
            }
        }
    }

    SaleRepository::NewSale ns;
    ns.clientName = clientName;
    ns.vendedor = vendedor.isEmpty() ? QStringLiteral("vendedor") : vendedor;
    ns.subtotal = totals.value().subtotal;
    ns.discount = totals.value().discount + promoDiscount;
    ns.tax = totals.value().tax;
    ns.total = total;
    ns.promoCode = promoUsed;
    ns.payments = payments;
    ns.paymentMethod = paymentMethod.isEmpty() ? QStringLiteral("Efectivo") : paymentMethod;
    ns.offline = offline;
    ns.taxBreakdownJson = bucketsToJson(totals.value().buckets);
    // Multitienda: rubro de la venta desde sus líneas (un solo rubro
    // específico → ese; mezcla o legacy → '' = visible en todos).
    {
        QSet<QString> rubros;
        for (const LineTotal &l : totals.value().lines) {
            const auto prod = m_products ? m_products->findById(l.productId) : std::nullopt;
            if (!prod)
                continue;
            const QString pb = prod->businessType.trimmed();
            if (!pb.isEmpty() && pb != QLatin1String("miscelanea"))
                rubros.insert(pb);
        }
        if (rubros.size() == 1)
            ns.businessType = *rubros.begin();
    }
    for (const LineTotal &l : totals.value().lines) {
        SaleItem it;
        it.productId = l.productId;
        it.qty = l.qty;
        it.subtotal = l.subtotal;
        it.serial = l.serial;
        // Fase 3: la receta viaja en la línea (auditable por venta).
        if (!l.receta.isEmpty())
            it.attrsJson = Attrs::set(QStringLiteral("{}"), Attrs::KReceta, l.receta);
        ns.items << it;
    }
    auto created = m_sales->create(ns);
    if (!created.ok())
        return Result<CreatedSale>::failure(created.error());
    const Sale &s = created.value();
    // Fase 3: la promo usada cuenta un uso (solo en ventas exitosas).
    // Best-effort: la venta ya quedó firme; si el conteo falla se audita.
    if (!promoUsed.isEmpty() && m_promos && !m_promos->registerUse(promoUsed) && m_audit)
        m_audit->log(ns.vendedor, QStringLiteral("promo_uso_no_contado"), promoUsed);
    // Fase 3: lo vendido consume apartados primero (sin fugas de reserva).
    if (m_products) {
        for (const LineTotal &l : totals.value().lines)
            m_products->releaseAtomic(l.productId, l.qty);
    }

    // Fase 3: marcar seriales como vendidos. Si alguno falla (carrera), se
    // cancela la venta completa para no dejar stock/serial inconsistente.
    if (m_serials) {
        for (const LineTotal &l : totals.value().lines) {
            if (l.serial.isEmpty())
                continue;
            if (!m_serials->sell(l.serial, s.id).ok()) {
                cancel(s.id, QStringLiteral("reserva de serial fallida"), ns.vendedor);
                return Result<CreatedSale>::failure(
                    QStringLiteral("Serial %1 ya no disponible; venta cancelada").arg(l.serial));
            }
        }
    }
    // Fase 3: auditoría de recetas.
    if (m_audit) {
        for (const LineTotal &l : totals.value().lines) {
            if (!l.receta.isEmpty())
                m_audit->log(ns.vendedor, QStringLiteral("venta_receta"),
                             QStringLiteral("%1 %2 receta %3").arg(s.id, l.sku, l.receta));
        }
    }

    // Descontar inventario + movimientos "Salida" tipo sale.
    // (SaleRepository::create ya decrementó el stock; aquí solo se registra
    // el movimiento con before/after derivados. Fase 2: double con epsilon.)
    for (const LineTotal &l : totals.value().lines) {
        const auto p = m_products->findById(l.productId);
        if (!p)
            continue;
        const double after = p->stock;
        const double before = after + l.qty;
        m_inventory->record(p->sku, p->name, QStringLiteral("Salida"), -l.qty, before, after,
                            QStringLiteral("Venta %1").arg(s.id), ns.vendedor);
        if (m_bus)
            m_bus->publish(EventBus::InventoryUpdated,
                           {{"product_id", l.productId}, {"quantity_change", -l.qty}});
    }
    if (m_bus)
        m_bus->publish(EventBus::SaleCreated,
                       {{"sale_id", s.id}, {"total", s.total}, {"client", clientName}});

    CreatedSale out;
    out.id = s.id;
    out.total = s.total;
    out.cufe = s.dianCufe;
    out.status = s.status;
    out.items = totals.value().lines;
    out.buckets = totals.value().buckets;
    return Result<CreatedSale>::success(out);
}

Result<SalesService::CreatedSale> SalesService::cancel(const QString &saleId, const QString &reason,
                                                       const QString &user, const QString &role)
{
    Q_UNUSED(reason);
    // Fase 3: anular exige supervisor (fail-closed: sin rol no se anula).
    if (role.trimmed() != QLatin1String("Administrador"))
        return Result<CreatedSale>::failure(
            QStringLiteral("Anular ventas requiere rol Administrador"));
    const auto s = m_sales->find(saleId);
    if (!s)
        return Result<CreatedSale>::failure(QStringLiteral("Venta %1 no existe").arg(saleId));
    if (s->estado == QLatin1String("Cancelada") || s->status == QLatin1String("Cancelada"))
        return Result<CreatedSale>::failure(
            QStringLiteral("Venta %1 ya está cancelada").arg(saleId));

    // Revertir inventario por línea
    for (const SaleItem &it : m_sales->itemsFor(saleId)) {
        const auto p = m_products->findById(it.productId);
        if (!p)
            continue;
        const double before = p->stock;
        m_products->setStockById(it.productId, before + it.qty);
        m_inventory->record(p->sku, p->name, QStringLiteral("Devolución"), it.qty, before,
                            before + it.qty, QStringLiteral("Cancelación venta %1").arg(saleId),
                            user);
    }
    // Fase 3: devolver seriales a in_stock.
    if (m_serials)
        m_serials->revertSale(saleId);
    auto adv = m_sales->advanceStatus(saleId, QStringLiteral("Cancelada"), user);
    if (!adv.ok())
        return Result<CreatedSale>::failure(adv.error());
    CreatedSale out;
    out.id = adv.value().id;
    out.total = adv.value().total;
    out.status = adv.value().status;
    return Result<CreatedSale>::success(out);
}
