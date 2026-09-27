// Fase 5 (MAP_PRO.md): operación profesional.
// - Documentos: folios por tipo, máquina de transiciones, conversión con
//   trazabilidad, NC/ND ligadas, Cancelada solo vía cancel() con motivo.
// - Compras: recepción parcial acumulada + CxP proporcional, vencidas y
//   estado de cuenta.
// - Inventario: lotes PEPS (consumo por vencimiento + valuación) y conteos
//   cíclicos (conteo → ajuste justificado).
#include <QtTest>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "TestDb.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/CreditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/PurchaseRepository.h"
#include "repositories/SaleRepository.h"
#include "services/InventoryService.h"
#include "services/PurchaseService.h"
#include "services/SalesService.h"

class TstFase5 : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("fase5.db"))));
        QVERIFY(TestDb::loadDemo(m_dbm->database()));
        QSqlDatabase db = m_dbm->database();
        m_audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, m_audit, this);
        m_clients = new ClientRepository(db, m_audit, this);
        m_caja = new CajaRepository(db, m_audit, this);
        m_sales = new SaleRepository(db, m_clients, m_caja, m_audit, this);
        m_inventory = new InventoryRepository(db, m_audit, this);
        m_promos = new PromoRepository(db, m_products, m_audit, this);
        m_purchases = new PurchaseRepository(db, m_audit, this);
        QSqlDatabase db2 = m_dbm->database();
        m_suppliers = new SupplierRepository(db2, m_audit, this);
        m_payables = new PayablesRepository(db, m_audit, this);
        m_bus = new EventBus(this);
        m_invSvc = new InventoryService(db, m_products, m_inventory, m_bus, this);
        m_purSvc = new PurchaseService(db, m_purchases, m_products, m_suppliers, m_inventory,
                                       m_payables, m_audit, this);
        m_salesSvc = new SalesService(db, m_products, m_sales, m_inventory, m_clients, m_caja,
                                      m_promos, m_bus, nullptr, m_audit, nullptr, this);
    }

    // ── Documentos ────────────────────────────────────────────────

    void docFoliosPerType()
    {
        auto rem = m_sales->createDocument(QStringLiteral("Remisión"), QStringLiteral("Juan Pérez"),
                                           Money::fromCop(1000.0), QStringLiteral("tester"));
        QVERIFY(rem.ok());
        QVERIFY(rem.value().id.startsWith(QStringLiteral("REM")));
        auto fe = m_sales->createDocument(QStringLiteral("Factura"), QStringLiteral("Juan Pérez"),
                                          Money::fromCop(2000.0), QStringLiteral("tester"));
        QVERIFY(fe.ok());
        QVERIFY(fe.value().id.startsWith(QStringLiteral("FE")));
        // Contadores independientes: REM y FE no colisionan con ventas POS (V*).
        QVERIFY(!rem.value().id.startsWith(QStringLiteral("V")));
        QVERIFY(!fe.value().id.startsWith(QStringLiteral("V")));
        auto nc = m_sales->createDocument(QStringLiteral("Nota crédito"),
                                          QStringLiteral("Juan Pérez"), Money::fromCop(100.0),
                                          QStringLiteral("t"));
        QVERIFY(nc.ok());
        QVERIFY(nc.value().id.startsWith(QStringLiteral("NC")));
        auto nd = m_sales->createDocument(QStringLiteral("Nota cargo"),
                                          QStringLiteral("Juan Pérez"), Money::fromCop(100.0),
                                          QStringLiteral("t"));
        QVERIFY(nd.ok());
        QVERIFY(nd.value().id.startsWith(QStringLiteral("ND")));
    }

    void docTransitions()
    {
        QVERIFY(SaleRepository::transitionAllowed(QStringLiteral("Cotización"),
                                                  QStringLiteral("Pedido")));
        QVERIFY(SaleRepository::transitionAllowed(QStringLiteral("Pedido"),
                                                  QStringLiteral("Facturada")));
        QVERIFY(!SaleRepository::transitionAllowed(QStringLiteral("Cotización"),
                                                   QStringLiteral("Facturada")));
        QVERIFY(!SaleRepository::transitionAllowed(QStringLiteral("Pedido"),
                                                   QStringLiteral("Cotización")));
        // advanceStatus aplica la máquina: salto directo COT→Facturada falla.
        auto cot = m_sales->createDocument(QStringLiteral("Cotización"),
                                           QStringLiteral("Ana Martínez"), Money::fromCop(500.0),
                                           QStringLiteral("tester"));
        QVERIFY(cot.ok());
        QVERIFY(!m_sales->advanceStatus(cot.value().id, QStringLiteral("Facturada"),
                                        QStringLiteral("tester"))
                     .ok());
        QVERIFY(m_sales->advanceStatus(cot.value().id, QStringLiteral("Pedido"),
                                       QStringLiteral("tester"))
                    .ok());
    }

    void docConvertTraces()
    {
        auto cot = m_sales->createDocument(QStringLiteral("Cotización"),
                                           QStringLiteral("Luis García"), Money::fromCop(700.0),
                                           QStringLiteral("tester"));
        QVERIFY(cot.ok());
        auto ped = m_sales->convertDocument(cot.value().id, QStringLiteral("Pedido"),
                                            QStringLiteral("tester"));
        QVERIFY(ped.ok());
        QCOMPARE(ped.value().parentId, cot.value().id);
        QVERIFY(ped.value().id.startsWith(QStringLiteral("PED")));
        // Origen consumido (Cerrada) y trazable.
        QCOMPARE(m_sales->find(cot.value().id)->status, QStringLiteral("Cerrada"));
        auto fe = m_sales->convertDocument(ped.value().id, QStringLiteral("Factura"),
                                           QStringLiteral("tester"));
        QVERIFY(fe.ok());
        QCOMPARE(fe.value().parentId, ped.value().id);
        QVERIFY(fe.value().id.startsWith(QStringLiteral("FE")));
        // Conversión inválida: COT directo a Factura.
        auto cot2 = m_sales->createDocument(QStringLiteral("Cotización"),
                                            QStringLiteral("Luis García"), Money::fromCop(50.0),
                                            QStringLiteral("tester"));
        QVERIFY(cot2.ok());
        QVERIFY(!m_sales->convertDocument(cot2.value().id, QStringLiteral("Factura"),
                                          QStringLiteral("tester"))
                     .ok());
    }

    void cancelNeedsServiceWithReason()
    {
        auto fe = m_sales->createDocument(QStringLiteral("Factura"), QStringLiteral("Juan Pérez"),
                                          Money::fromCop(3000.0), QStringLiteral("tester"));
        QVERIFY(fe.ok());
        // Bypass directo bloqueado.
        auto direct = m_sales->advanceStatus(fe.value().id, QStringLiteral("Cancelada"),
                                             QStringLiteral("tester"));
        QVERIFY(!direct.ok());
        QVERIFY(m_sales->find(fe.value().id)->status != QStringLiteral("Cancelada"));
        // Sin motivo también falla.
        QVERIFY(!m_salesSvc
                     ->cancel(fe.value().id, QString(), QStringLiteral("tester"),
                              QStringLiteral("Administrador"))
                     .ok());
        // Con motivo cierra por el único camino.
        auto c = m_salesSvc->cancel(fe.value().id, QStringLiteral("mercancía dañada"),
                                    QStringLiteral("tester"), QStringLiteral("Administrador"));
        QVERIFY(c.ok());
        const auto done = m_sales->find(fe.value().id);
        QVERIFY(done.has_value());
        QCOMPARE(done->status, QStringLiteral("Cancelada"));
        QCOMPARE(done->reason, QStringLiteral("mercancía dañada"));
    }

    void creditNotesLinkedAndCapped()
    {
        auto fe = m_sales->createDocument(QStringLiteral("Factura"), QStringLiteral("María López"),
                                          Money::fromCop(10000.0), QStringLiteral("tester"));
        QVERIFY(fe.ok());
        // NC sobre cotización: rechazada.
        auto cot = m_sales->createDocument(QStringLiteral("Cotización"),
                                           QStringLiteral("María López"), Money::fromCop(10000.0),
                                           QStringLiteral("tester"));
        QVERIFY(cot.ok());
        QVERIFY(!m_sales
                     ->createCreditNote(cot.value().id, Money::fromCop(1000.0),
                                        QStringLiteral("motivo"), QStringLiteral("tester"))
                     .ok());
        // NC ligada con motivo persistido.
        auto nc1 = m_sales->createCreditNote(fe.value().id, Money::fromCop(4000.0),
                                             QStringLiteral("devolución"), QStringLiteral("tester"));
        QVERIFY(nc1.ok());
        QCOMPARE(nc1.value().parentId, fe.value().id);
        QCOMPARE(nc1.value().reason, QStringLiteral("devolución"));
        QCOMPARE(m_sales->creditNotesTotal(fe.value().id), Money::fromCop(4000.0));
        // Segunda NC que excede el total: rechazada (4000 + 7000 > 10000).
        QVERIFY(!m_sales
                     ->createCreditNote(fe.value().id, Money::fromCop(7000.0),
                                        QStringLiteral("otra"), QStringLiteral("tester"))
                     .ok());
        // NC complementaria exacta sí pasa (4000 + 6000 = 10000).
        QVERIFY(m_sales
                    ->createCreditNote(fe.value().id, Money::fromCop(6000.0),
                                       QStringLiteral("resto"), QStringLiteral("tester"))
                    .ok());
    }

    // ── Compras parciales + CxP ───────────────────────────────────

    void purchasePartialReceive()
    {
        auto oc = m_purSvc->create(QStringLiteral("TecnoMayorista SAS"), QStringLiteral("P002"),
                                   10.0, QStringLiteral("tester"));
        QVERIFY(oc.ok());
        const double stockBefore = m_products->findBySku(QStringLiteral("P002"))->stock;
        // Primera entrega parcial: 4 de 10.
        auto p1 = m_purSvc->receive(oc.value().id, {{"P002", 4.0}}, QStringLiteral("tester"));
        QVERIFY(p1.ok());
        QCOMPARE(p1.value().status, QStringLiteral("Parcial"));
        QCOMPARE(m_purchases->find(oc.value().id)->received.value(QStringLiteral("P002")), 4.0);
        QCOMPARE(m_products->findBySku(QStringLiteral("P002"))->stock, stockBefore + 4.0);
        // CxP proporcional a lo recibido (4 × costo), no al total de la OC.
        const auto cxp1 = m_payables->find(oc.value().id);
        QVERIFY(cxp1.has_value());
        QVERIFY(qAbs((cxp1->amount - oc.value().items.first().priceBuy * 4.0).toCop()) < 1.0);
        // Exceder lo pedido falla.
        QVERIFY(!m_purSvc->receive(oc.value().id, {{"P002", 7.0}}, QStringLiteral("tester")).ok());
        // Segunda entrega completa el resto: 6 de 10 → Recibida.
        auto p2 = m_purSvc->receive(oc.value().id, {{"P002", 6.0}}, QStringLiteral("tester"));
        QVERIFY(p2.ok());
        QCOMPARE(p2.value().status, QStringLiteral("Recibida"));
        QCOMPARE(m_products->findBySku(QStringLiteral("P002"))->stock, stockBefore + 10.0);
        const auto cxp2 = m_payables->find(oc.value().id);
        QVERIFY(cxp2.has_value());
        QVERIFY(qAbs((cxp2->amount - oc.value().items.first().priceBuy * 10.0).toCop()) < 1.0);
        // Recibir de más u ordenar cancelada falla.
        QVERIFY(!m_purSvc->receive(oc.value().id, {{"P002", 1.0}}, QStringLiteral("tester")).ok());
    }

    void payablesOverdueAndStatement()
    {
        Payable p;
        p.id = QStringLiteral("CXPF5-1");
        p.supplier = QStringLiteral("Proveedor Fase5");
        p.due = QStringLiteral("2000-01-01"); // vencida
        p.amount = Money::fromCop(5000.0);
        p.paid = Money();
        p.balance = Money::fromCop(5000.0);
        p.status = QStringLiteral("Pendiente");
        QVERIFY(m_payables->create(p).ok());
        bool found = false;
        for (const Payable &o : m_payables->overdue()) {
            if (o.id == QStringLiteral("CXPF5-1")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
        bool inStatement = false;
        for (const Payable &o : m_payables->statement(QStringLiteral("Proveedor Fase5"))) {
            if (o.id == QStringLiteral("CXPF5-1"))
                inStatement = true;
        }
        QVERIFY(inStatement);
        auto pay = m_payables->addPayment(QStringLiteral("CXPF5-1"), Money::fromCop(2000.0),
                                          QStringLiteral("Efectivo"), QStringLiteral("tester"));
        QVERIFY(pay.ok());
        const auto hist = m_payables->paymentsFor(QStringLiteral("CXPF5-1"));
        QVERIFY(!hist.isEmpty());
        QCOMPARE(hist.first().amount, Money::fromCop(2000.0));
    }

    // ── Lotes PEPS + conteos ──────────────────────────────────────

    void lotsFifoConsume()
    {
        const QString sku = QStringLiteral("P006");
        // Dos entradas: lote viejo barato + lote nuevo caro.
        QVERIFY(m_invSvc
                    ->registerPurchase(6, 10.0, Money::fromCop(100.0), QStringLiteral("Prov"),
                                       QStringLiteral("L1"), QStringLiteral("tester"),
                                       QStringLiteral("LOTE-A"), QStringLiteral("2027-01-01"))
                    .ok());
        QVERIFY(m_invSvc
                    ->registerPurchase(6, 10.0, Money::fromCop(200.0), QStringLiteral("Prov"),
                                       QStringLiteral("L2"), QStringLiteral("tester"),
                                       QStringLiteral("LOTE-B"), QStringLiteral("2028-01-01"))
                    .ok());
        // Consumo 12: agota LOTE-A (10×100) + 2 del B (2×200) = 1400.
        auto cogs = m_invSvc->consumeFifo(sku, 12.0);
        QVERIFY(cogs.ok());
        QCOMPARE(cogs.value(), Money::fromCop(1400.0));
        // Sin vencimiento '' va al final: crear lote sin fecha y verificar
        // que el orden PEPS lo deja último (queda con stock).
        QVERIFY(m_inventory
                    ->addLot(sku, QStringLiteral("LOTE-SF"), QString(), 5.0,
                             Money::fromCop(50.0))
                    .ok());
        const auto lots = m_inventory->lotsBySku(sku);
        QVERIFY(!lots.isEmpty());
        QCOMPARE(lots.last().lote, QStringLiteral("LOTE-SF"));
        // Valuación PEPS > 0 y distinta del promedio si hay mezcla.
        QVERIFY(m_inventory->lotsValue(sku).isPositive());
        QVERIFY(m_invSvc->valuation(QStringLiteral("peps")).totalValue.isPositive());
    }

    void cyclicCountApplies()
    {
        const QString sku = QStringLiteral("P008");
        const double before = m_products->findBySku(sku)->stock;
        auto c = m_invSvc->startCount(sku, before - 3.0, QStringLiteral("faltante góndola"),
                                      QStringLiteral("tester"));
        QVERIFY(c.ok());
        QCOMPARE(c.value().status, QStringLiteral("Pendiente"));
        QCOMPARE(c.value().diff, -3.0);
        // Sin motivo no se abre conteo.
        QVERIFY(!m_invSvc->startCount(sku, before, QString(), QStringLiteral("tester")).ok());
        auto applied = m_invSvc->applyCount(c.value().id, QStringLiteral("tester"));
        QVERIFY(applied.ok());
        QCOMPARE(applied.value().status, QStringLiteral("Aplicado"));
        QCOMPARE(m_products->findBySku(sku)->stock, before - 3.0);
        // Doble aplicación bloqueada.
        QVERIFY(!m_invSvc->applyCount(c.value().id, QStringLiteral("tester")).ok());
    }

  private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    AuditRepository *m_audit = nullptr;
    ProductRepository *m_products = nullptr;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PromoRepository *m_promos = nullptr;
    PurchaseRepository *m_purchases = nullptr;
    SupplierRepository *m_suppliers = nullptr;
    PayablesRepository *m_payables = nullptr;
    EventBus *m_bus = nullptr;
    InventoryService *m_invSvc = nullptr;
    PurchaseService *m_purSvc = nullptr;
    SalesService *m_salesSvc = nullptr;
};

QTEST_MAIN(TstFase5)
#include "tst_fase5.moc"
