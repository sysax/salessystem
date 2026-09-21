#include "SalesService.h"

#include "../core/EventBus.h"

SalesService::SalesService(QSqlDatabase db, ProductRepository *products, SaleRepository *sales,
                           InventoryRepository *inventory, ClientRepository *clients,
                           CajaRepository *caja, PromoRepository *promos, EventBus *bus,
                           QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_sales(sales),
      m_inventory(inventory), m_clients(clients), m_caja(caja), m_promos(promos), m_bus(bus)
{
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

Result<SalesService::Totals> SalesService::buildTotals(const QList<ServiceItem> &items,
                                                       QString &error) const
{
    Totals t;
    t.itemsCount = items.size();
    for (const ServiceItem &it : items) {
        const auto p = m_products->findById(it.productId);
        if (!p) {
            error = QStringLiteral("Producto ID %1 no existe").arg(it.productId);
            return Result<Totals>::failure(error);
        }
        if (p->stock < it.qty) {
            error = QStringLiteral("Stock insuficiente para '%1'. Disponible: %2, Solicitado: %3")
                        .arg(p->name)
                        .arg(p->stock)
                        .arg(it.qty);
            return Result<Totals>::failure(error);
        }
        LineTotal l;
        l.productId = p->id;
        l.name = p->name;
        l.sku = p->sku;
        l.qty = it.qty;
        l.unitPrice = it.priceOverride > 0 ? it.priceOverride : p->price;
        l.subtotal = l.unitPrice * it.qty;
        l.discount = l.subtotal * it.discountPct / 100.0;
        l.tax = (l.subtotal - l.discount) * parseTaxRate(p->tax) / 100.0;
        l.total = l.subtotal - l.discount + l.tax;
        t.lines << l;
        t.subtotal += l.subtotal;
        t.discount += l.discount;
        t.tax += l.tax;
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
        l.unitPrice = it.priceOverride > 0 ? it.priceOverride : p->price;
        l.subtotal = l.unitPrice * it.qty;
        l.discount = l.subtotal * it.discountPct / 100.0;
        l.tax = (l.subtotal - l.discount) * parseTaxRate(p->tax) / 100.0;
        l.total = l.subtotal - l.discount + l.tax;
        t.lines << l;
        t.subtotal += l.subtotal;
        t.discount += l.discount;
        t.tax += l.tax;
    }
    t.itemsCount = items.size();
    t.total = t.subtotal - t.discount + t.tax;
    return t;
}

Result<SalesService::CreatedSale> SalesService::create(
    const QList<ServiceItem> &items, const QString &clientName,
    const QMap<QString, double> &payments, const QString &paymentMethod,
    const QString &promoCode, const QString &vendedor, bool offline)
{
    if (items.isEmpty())
        return Result<CreatedSale>::failure(
            QStringLiteral("La venta debe tener al menos un item"));

    QString error;
    auto totals = buildTotals(items, error);
    if (!totals.ok())
        return Result<CreatedSale>::failure(totals.error());

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
    for (const LineTotal &l : totals.value().lines) {
        SaleItem it;
        it.productId = l.productId;
        it.qty = l.qty;
        it.subtotal = l.subtotal;
        ns.items << it;
    }
    auto created = m_sales->create(ns);
    if (!created.ok())
        return Result<CreatedSale>::failure(created.error());
    const Sale &s = created.value();

    // Descontar inventario + movimientos "Salida" tipo sale.
    // (SaleRepository::create ya decrementó el stock; aquí solo se registra
    // el movimiento con before/after derivados.)
    for (const LineTotal &l : totals.value().lines) {
        const auto p = m_products->findById(l.productId);
        if (!p)
            continue;
        const int after = p->stock;
        const int before = after + l.qty;
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
        const int before = p->stock;
        m_products->setStockById(it.productId, before + it.qty);
        m_inventory->record(p->sku, p->name, QStringLiteral("Devolución"), it.qty, before,
                            before + it.qty,
                            QStringLiteral("Cancelación venta %1").arg(saleId), user);
    }
    auto adv = m_sales->advanceStatus(saleId, QStringLiteral("Cancelada"), user);
    if (!adv.ok())
        return Result<CreatedSale>::failure(adv.error());
    CreatedSale out;
    out.id = adv.value().id;
    out.total = adv.value().total;
    out.status = adv.value().status;
    return Result<CreatedSale>::success(out);
}
