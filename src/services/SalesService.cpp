#include "SalesService.h"

#include "../core/EventBus.h"
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

namespace
{
// Agrega una línea al bucket de su tasa (agregación, sin recalcular).
void accumulateBucket(QList<SalesService::TaxBucket> &buckets, const QString &name,
                      double rate, double lineBase, double lineTax)
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
                                                        QString &error) const
{
    Totals t;
    t.itemsCount = items.size();
    const QDate today = QDate::currentDate();
    QSet<QString> usedSerials;
    for (const ServiceItem &it : items) {
        const auto p = m_products->findById(it.productId);
        if (!p) {
            error = QStringLiteral("Producto ID %1 no existe").arg(it.productId);
            return Result<Totals>::failure(error);
        }
        if (p->stock < it.qty - 1e-9) {
            error = QStringLiteral("Stock insuficiente para '%1'. Disponible: %2, Solicitado: %3")
                        .arg(p->name)
                        .arg(p->stock)
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
                    error = QStringLiteral("Serial %1 no disponible (%2)")
                                .arg(serial, s->status);
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
        l.unitPrice = it.priceOverride > 0 ? it.priceOverride : p->price;
        l.subtotal = l.unitPrice * it.qty;
        l.discount = l.subtotal * it.discountPct / 100.0;
        l.taxRate = resolveTaxRate(p->tax);
        l.taxName = resolveTaxName(p->tax);
        l.tax = (l.subtotal - l.discount) * l.taxRate / 100.0;
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
        l.tax = (l.subtotal - l.discount) * l.taxRate / 100.0;
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

Result<SalesService::CreatedSale> SalesService::create(
    const QList<ServiceItem> &items, const QString &clientName,
    const QMap<QString, double> &payments, const QString &paymentMethod,
    const QString &promoCode, const QString &vendedor, bool offline, const QString &role)
{
    if (items.isEmpty())
        return Result<CreatedSale>::failure(
            QStringLiteral("La venta debe tener al menos un item"));

    QString error;
    auto totals = buildTotals(items, error);
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
        auto promo = m_promos->evaluate(cart, promoCode);
        if (!promo.ok())
            return Result<CreatedSale>::failure(promo.error());
        promoDiscount = promo.value().discount;
        promoUsed = promo.value().promoCode;
    }
    const double total = totals.value().total - promoDiscount;

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

    // Fase 3: marcar seriales como vendidos. Si alguno falla (carrera), se
    // cancela la venta completa para no dejar stock/serial inconsistente.
    if (m_serials) {
        for (const LineTotal &l : totals.value().lines) {
            if (l.serial.isEmpty())
                continue;
            if (!m_serials->sell(l.serial, s.id).ok()) {
                cancel(s.id, QStringLiteral("reserva de serial fallida"),
                       ns.vendedor);
                return Result<CreatedSale>::failure(
                    QStringLiteral("Serial %1 ya no disponible; venta cancelada")
                        .arg(l.serial));
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
        m_inventory->record(p->sku, p->name, QStringLiteral("Salida"), -l.qty, before,
                            after, QStringLiteral("Venta %1").arg(s.id), ns.vendedor);
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

Result<SalesService::CreatedSale> SalesService::cancel(const QString &saleId,
                                                       const QString &reason,
                                                       const QString &user)
{
    Q_UNUSED(reason);
    const auto s = m_sales->find(saleId);
    if (!s)
        return Result<CreatedSale>::failure(
            QStringLiteral("Venta %1 no existe").arg(saleId));
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
                            before + it.qty,
                            QStringLiteral("Cancelación venta %1").arg(saleId), user);
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
