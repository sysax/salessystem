#include "PosController.h"

PosController::PosController(SalesService *sales, ProductRepository *products,
                             PromoRepository *promos, CajaRepository *caja,
                             TicketPrinter *printer, SyncService *sync, QObject *parent)
    : QObject(parent), m_sales(sales), m_products(products), m_promos(promos), m_caja(caja),
      m_printer(printer), m_sync(sync)
{
    refreshCaja();
}

int PosController::pendingSync() const
{
    return m_sync ? m_sync->pendingCount() : 0;
}

QVariantMap PosController::addToCart(int productId, int qty)
{
    if (qty <= 0)
        return {{"ok", false}, {"error", QStringLiteral("Cantidad >0")}};
    const auto p = m_products->findById(productId);
    if (!p)
        return {{"ok", false}, {"error", QStringLiteral("Producto no existe")}};
    // Acumular si ya está en el carrito
    for (QVariant &v : m_cart) {
        QVariantMap line = v.toMap();
        if (line["productId"].toInt() == productId) {
            const int q = line["qty"].toInt() + qty;
            if (p->stock < q)
                return {{"ok", false},
                        {"error", QStringLiteral("Stock insuficiente: %1").arg(p->stock)}};
            line["qty"] = q;
            line["subtotal"] = p->price * q;
            v = line;
            recompute();
            return {{"ok", true}};
        }
    }
    if (p->stock < qty)
        return {{"ok", false},
                {"error", QStringLiteral("Stock insuficiente: %1").arg(p->stock)}};
    m_cart << QVariantMap{{"productId", p->id},
                          {"sku", p->sku},
                          {"name", p->name},
                          {"price", p->price},
                          {"qty", qty},
                          {"subtotal", p->price * qty}};
    recompute();
    return {{"ok", true}};
}

void PosController::setQty(int index, int qty)
{
    if (index < 0 || index >= m_cart.size() || qty <= 0)
        return;
    QVariantMap line = m_cart[index].toMap();
    line["qty"] = qty;
    line["subtotal"] = line["price"].toDouble() * qty;
    m_cart[index] = line;
    recompute();
}

void PosController::removeLine(int index)
{
    if (index < 0 || index >= m_cart.size())
        return;
    m_cart.removeAt(index);
    recompute();
}

void PosController::clearCart()
{
    m_cart.clear();
    m_promoCode.clear();
    recompute();
}

void PosController::recompute()
{
    QList<SalesService::ServiceItem> items;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        items << SalesService::ServiceItem{line["productId"].toInt(), line["qty"].toInt()};
    }
    SalesService::Totals t = m_sales->calculateTotals(items);
    double promoDiscount = 0.0;
    if (!m_promoCode.isEmpty()) {
        QList<CartLine> cart;
        for (const auto &l : t.lines)
            cart << CartLine{l.productId, l.sku, {}, l.unitPrice, l.qty, l.subtotal};
        const auto pr = m_promos->evaluate(cart, m_promoCode);
        if (pr.ok())
            promoDiscount = pr.value().discount;
    }
    m_totals = {{"subtotal", t.subtotal},
                {"discount", t.discount + promoDiscount},
                {"tax", t.tax},
                {"total", t.total - promoDiscount}};
    emit cartChanged();
}

QVariantMap PosController::applyPromo(const QString &code)
{
    if (code.trimmed().isEmpty()) {
        m_promoCode.clear();
        recompute();
        return {{"ok", true}, {"discount", 0.0}};
    }
    QList<SalesService::ServiceItem> items;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        items << SalesService::ServiceItem{line["productId"].toInt(), line["qty"].toInt()};
    }
    const SalesService::Totals t = m_sales->calculateTotals(items);
    QList<CartLine> cart;
    for (const auto &l : t.lines)
        cart << CartLine{l.productId, l.sku, {}, l.unitPrice, l.qty, l.subtotal};
    const auto pr = m_promos->evaluate(cart, code);
    if (!pr.ok())
        return {{"ok", false}, {"error", pr.error()}};
    m_promoCode = pr.value().promoCode;
    recompute();
    return {{"ok", true}, {"discount", pr.value().discount}};
}

QVariantMap PosController::checkout(const QString &client, const QVariantMap &payments,
                                    const QString &method, const QString &user)
{
    if (m_cart.isEmpty())
        return {{"ok", false}, {"error", QStringLiteral("Carrito vacío")}};

    QList<SalesService::ServiceItem> items;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        items << SalesService::ServiceItem{line["productId"].toInt(), line["qty"].toInt()};
    }
    QMap<QString, double> pay;
    for (auto it = payments.begin(); it != payments.end(); ++it)
        pay[it.key().toLower()] = it.value().toDouble();

    const auto r = m_sales->create(items, client.isEmpty() ? QStringLiteral("Mostrador") : client,
                                   pay, method, m_promoCode, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};

    // Cambio: efectivo entregado − total (si hay pago en efectivo)
    const double cash = pay.value(QStringLiteral("efectivo"), 0.0);
    const double change = cash > r.value().total ? cash - r.value().total : 0.0;

    // Ticket .txt (offline-first: siempre se guarda)
    TicketPrinter::Ticket ticket;
    ticket.saleId = r.value().id;
    ticket.clientName = client;
    ticket.docType = QStringLiteral("Ticket de venta");
    ticket.cufe = r.value().cufe;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        TicketPrinter::Ticket::Line tl;
        tl.name = line["name"].toString();
        tl.qty = line["qty"].toInt();
        tl.price = line["price"].toDouble();
        tl.subtotal = line["subtotal"].toDouble();
        ticket.lines << tl;
    }
    ticket.discount = m_totals["discount"].toDouble();
    ticket.promoCode = m_promoCode;
    ticket.tax = m_totals["tax"].toDouble();
    ticket.total = r.value().total;
    ticket.payments = pay;
    ticket.change = change;
    QString ticketPath;
    if (const auto pr = m_printer->print(ticket); pr.ok())
        ticketPath = pr.value().path;

    // Encolar para sincronizar al reconectar
    if (m_sync) {
        m_sync->queueOperation(QStringLiteral("sale"),
                               {{"folio", r.value().id}, {"total", r.value().total}});
        emit syncChanged();
    }

    clearCart();
    refreshCaja();
    return {{"ok", true},
            {"saleId", r.value().id},
            {"total", r.value().total},
            {"change", change},
            {"ticket", ticketPath}};
}

QVariantMap PosController::openCaja(double amount, const QString &user)
{
    const auto r = m_caja->open(amount, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refreshCaja();
    return {{"ok", true}};
}

QVariantMap PosController::closeCaja(double counted, const QString &user)
{
    const auto r = m_caja->close(counted, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refreshCaja();
    return {{"ok", true},
            {"expected", r.value().expected},
            {"diff", r.value().diff},
            {"sales", r.value().salesCount}};
}

void PosController::refreshCaja()
{
    const CajaStatus st = m_caja->status();
    m_cajaStatus = {{"open", st.open},
                    {"openingAmount", st.openingAmount},
                    {"openingUser", st.openingUser},
                    {"totalSales", st.totalSales},
                    {"expected", st.expected},
                    {"salesCount", st.salesToday.size()}};
    emit cajaChanged();
}
