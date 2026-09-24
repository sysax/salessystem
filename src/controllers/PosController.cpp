#include "PosController.h"

#include "../domain/Attrs.h"

namespace
{
// Fase 3: producto con seguimiento de serial.
bool lineTracked(const Product &p, const SerialRepository *serials)
{
    if (Attrs::boolean(p.attrsJson, Attrs::KTrackSerial))
        return true;
    return serials && serials->hasSerials(p.sku);
}
} // namespace

PosController::PosController(SalesService *sales, ProductRepository *products,
                             PromoRepository *promos, CajaRepository *caja,
                             TicketPrinter *printer, SyncService *sync,
                             SettingsService *settings, SerialRepository *serials,
                             QObject *parent)
    : QObject(parent), m_sales(sales), m_products(products), m_promos(promos), m_caja(caja),
      m_printer(printer), m_sync(sync), m_settings(settings), m_serials(serials)
{
    refreshCaja();
}

int PosController::pendingSync() const
{
    return m_sync ? m_sync->pendingCount() : 0;
}

QVariantMap PosController::addToCart(int productId, double qty)
{
    if (qty <= 1e-9)
        return {{"ok", false}, {"error", QStringLiteral("Cantidad >0")}};
    const auto p = m_products->findById(productId);
    if (!p)
        return {{"ok", false}, {"error", QStringLiteral("Producto no existe")}};
    // Fase 3: productos tracked van 1 por línea (un serial por unidad).
    if (lineTracked(*p, m_serials)) {
        if (m_serials && m_serials->inStockCount(p->sku) <= cartSerialLines(productId))
            return {{"ok", false},
                    {"error", QStringLiteral("Sin seriales disponibles para '%1'").arg(p->name)}};
        if (p->stock < cartQtyFor(productId) + 1.0 - 1e-9)
            return {{"ok", false},
                    {"error", QStringLiteral("Stock insuficiente: %1").arg(p->stock)}};
        m_cart << QVariantMap{{"productId", p->id},
                              {"sku", p->sku},
                              {"name", p->name},
                              {"price", p->price},
                              {"unit", p->unit},
                              {"qty", 1.0},
                              {"subtotal", p->price},
                              {"serial", QString()},
                              {"receta", QString()}};
        recompute();
        QVariantMap ok{{"ok", true}};
        ok["needsSerial"] = true;
        return ok;
    }
    // Acumular si ya está en el carrito
    for (QVariant &v : m_cart) {
        QVariantMap line = v.toMap();
        if (line["productId"].toInt() == productId) {
            const double q = line["qty"].toDouble() + qty;
            if (p->stock < q - 1e-9)
                return {{"ok", false},
                        {"error", QStringLiteral("Stock insuficiente: %1").arg(p->stock)}};
            line["qty"] = q;
            line["subtotal"] = p->price * q;
            v = line;
            recompute();
            return {{"ok", true}};
        }
    }
    if (p->stock < qty - 1e-9)
        return {{"ok", false},
                {"error", QStringLiteral("Stock insuficiente: %1").arg(p->stock)}};
    m_cart << QVariantMap{{"productId", p->id},
                          {"sku", p->sku},
                          {"name", p->name},
                          {"price", p->price},
                          {"unit", p->unit},
                          {"qty", qty},
                          {"subtotal", p->price * qty},
                          {"serial", QString()},
                          {"receta", QString()}};
    recompute();
    return {{"ok", true}};
}

void PosController::setQty(int index, double qty)
{
    if (index < 0 || index >= m_cart.size() || qty <= 1e-9)
        return;
    QVariantMap line = m_cart[index].toMap();
    // Fase 4: línea con serial = 1 unidad fija (un serial por equipo).
    if (!line.value(QStringLiteral("serial")).toString().trimmed().isEmpty())
        return;
    if (const auto p = m_products->findById(line["productId"].toInt());
        p && lineTracked(*p, m_serials))
        return;
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

int PosController::cartSerialLines(int productId) const
{
    int n = 0;
    for (const QVariant &v : m_cart) {
        if (v.toMap()["productId"].toInt() == productId)
            ++n;
    }
    return n;
}

double PosController::cartQtyFor(int productId) const
{
    double q = 0.0;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        if (line["productId"].toInt() == productId)
            q += line["qty"].toDouble();
    }
    return q;
}

QVariantMap PosController::setLineSerial(int index, const QString &serial)
{
    if (index < 0 || index >= m_cart.size())
        return {{"ok", false}, {"error", QStringLiteral("Línea inválida")}};
    const QString s = serial.trimmed();
    // Fase 4: validar al capturar (no solo al cobrar): registrado y en stock,
    // y sin repetir en el carrito.
    if (!s.isEmpty() && m_serials) {
        const auto found = m_serials->find(s);
        if (!found)
            return {{"ok", false},
                    {"error", QStringLiteral("Serial %1 no registrado").arg(s)}};
        if (found->status != QLatin1String("in_stock"))
            return {{"ok", false},
                    {"error", QStringLiteral("Serial %1 no disponible (%2)")
                                 .arg(s, found->status)}};
        for (int i = 0; i < m_cart.size(); ++i) {
            if (i != index
                && m_cart[i].toMap().value(QStringLiteral("serial")).toString() == s)
                return {{"ok", false},
                        {"error", QStringLiteral("Serial %1 repetido en la venta").arg(s)}};
        }
    }
    QVariantMap line = m_cart[index].toMap();
    line["serial"] = serial.trimmed();
    m_cart[index] = line;
    emit cartChanged();
    return {{"ok", true}};
}

QVariantMap PosController::setLineReceta(int index, const QString &receta)
{
    if (index < 0 || index >= m_cart.size())
        return {{"ok", false}, {"error", QStringLiteral("Línea inválida")}};
    QVariantMap line = m_cart[index].toMap();
    line["receta"] = receta.trimmed();
    m_cart[index] = line;
    emit cartChanged();
    return {{"ok", true}};
}

QVariantList PosController::inStockSerials(int productId) const
{
    QVariantList out;
    if (!m_serials)
        return out;
    const auto p = m_products->findById(productId);
    if (!p)
        return out;
    for (const SerialInfo &s : m_serials->inStock(p->sku))
        out << QVariantMap{{"serial", s.serial}, {"imei2", s.imei2}};
    return out;
}

void PosController::recompute()
{
    QList<SalesService::ServiceItem> items;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        SalesService::ServiceItem si{line["productId"].toInt(), line["qty"].toDouble()};
        si.serial = line.value(QStringLiteral("serial")).toString();
        si.receta = line.value(QStringLiteral("receta")).toString();
        items << si;
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
    QVariantList buckets;
    for (const auto &b : t.buckets) {
        buckets << QVariantMap{{"name", b.name},
                               {"rate", b.rate},
                               {"base", b.base},
                               {"tax", b.tax}};
    }
    m_totals = {{"subtotal", t.subtotal},
                {"discount", t.discount + promoDiscount},
                {"tax", t.tax},
                {"total", t.total - promoDiscount},
                {"taxBreakdown", buckets}};
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
        SalesService::ServiceItem si{line["productId"].toInt(), line["qty"].toDouble()};
        si.serial = line.value(QStringLiteral("serial")).toString();
        si.receta = line.value(QStringLiteral("receta")).toString();
        items << si;
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
                                    const QString &method, const QString &user,
                                    const QString &role)
{
    if (m_cart.isEmpty())
        return {{"ok", false}, {"error", QStringLiteral("Carrito vacío")}};

    QList<SalesService::ServiceItem> items;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        SalesService::ServiceItem si{line["productId"].toInt(), line["qty"].toDouble()};
        si.serial = line.value(QStringLiteral("serial")).toString();
        si.receta = line.value(QStringLiteral("receta")).toString();
        items << si;
    }
    QMap<QString, double> pay;
    for (auto it = payments.begin(); it != payments.end(); ++it)
        pay[it.key().toLower()] = it.value().toDouble();

    const auto r = m_sales->create(items, client.isEmpty() ? QStringLiteral("Mostrador") : client,
                                   pay, method, m_promoCode, user, false, role);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};

    // Cambio: efectivo entregado − total (si hay pago en efectivo)
    const double cash = pay.value(QStringLiteral("efectivo"), 0.0);
    const double change = cash > r.value().total ? cash - r.value().total : 0.0;

    // Ticket .txt (offline-first: siempre se guarda). Cabecera del negocio
    // desde SettingsService (Fase 0 multinegocio).
    TicketPrinter::Ticket ticket;
    ticket.saleId = r.value().id;
    ticket.clientName = client;
    if (m_settings) {
        ticket.businessName = m_settings->businessName();
        ticket.businessNit = m_settings->nit();
        ticket.businessAddress = m_settings->address();
        ticket.businessPhone = m_settings->phone();
        ticket.currencySymbol = m_settings->currencySymbol();
    }
    ticket.docType = QStringLiteral("Ticket de venta");
    ticket.cufe = r.value().cufe;
    for (const QVariant &v : m_cart) {
        const QVariantMap line = v.toMap();
        TicketPrinter::Ticket::Line tl;
        tl.name = line["name"].toString();
        tl.qty = line["qty"].toDouble();
        tl.price = line["price"].toDouble();
        tl.subtotal = line["subtotal"].toDouble();
        tl.serial = line.value(QStringLiteral("serial")).toString();
        ticket.lines << tl;
    }
    ticket.discount = m_totals["discount"].toDouble();
    ticket.promoCode = m_promoCode;
    ticket.tax = m_totals["tax"].toDouble();
    ticket.total = r.value().total;
    // Fase 1: desglose por tasa (desde el create, ya validado contra settings).
    for (const auto &b : r.value().buckets) {
        TicketPrinter::Ticket::TaxLine tl;
        tl.label = b.name;
        tl.base = b.base;
        tl.tax = b.tax;
        ticket.taxLines << tl;
    }
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
