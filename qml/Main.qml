// QtSalesSystem — shell principal (fase 4).
// Drawer + StackView con guardia de sesión y permisos por rol
// (antes SalesApp.navigate_to + refresh_sidebar).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "components"
import "Utils.js" as Utils
import QtSalesSystem

ApplicationWindow {
    id: root
    visible: true
    width: 1366
    height: 800
    title: qsTr("Sistema de Ventas")

    Component.onCompleted: {
        Utils.registerToast(globalToastItem);
        Utils.registerLoading(globalLoadingItem);
        root.tickClock();
    }

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.primary: Theme.primary   // menta pastel
    Material.accent: Theme.accent

    property string currentScreen: "login"

    // Header útil (mejora #12): reloj, estado de red y alertas.
    property string clockText: "--/-- --:--:--"
    property bool online: true

    function tickClock() {
        // Fase 7: formato según idioma de la app (no del sistema).
        var lang = "es";
        try { lang = settingsCtl.settings["language"] || "es"; } catch (e) {}
        try {
            clockText = Qt.formatDateTime(new Date(),
                lang === "en" ? "MM/dd hh:mm:ss AP" : "dd/MM HH:mm:ss");
        } catch (e) {
            clockText = Qt.formatDateTime(new Date(), "dd/MM HH:mm:ss");
        }
    }

    // Fase 7: atajos personalizables (settings shortcut_*; fallback = default).
    function shortcutSeq(key, fallback) {
        try {
            var v = settingsCtl.settings[key];
            if (v && String(v).trim() !== "")
                return String(v);
        } catch (e) {}
        return fallback;
    }

    function recheckOnline() {
        // isOnline() bloquea hasta 1.5s solo sin red; por eso se llama al login,
        // cada 120s y manualmente, nunca en cada navegación.
        online = syncSvc.isOnline();
        return online;
    }

    // Fase 6: tooltip con último sync, pendientes y error.
    function syncTip() {
        try {
            var m = syncSvc.metrics();
            var s = qsTr("Pendientes: %1").arg(m.pending);
            if (m.lastSync)
                s += qsTr(" · último sync: %1").arg(m.lastSync);
            if (m.lastError)
                s += qsTr(" · error: %1").arg(m.lastError);
            return s;
        } catch (e) {
            return qsTr("Estado de sincronización");
        }
    }

    // Mejora #5: metadatos para breadcrumbs + sidebar (etiqueta, icono, sección).
    function screenMeta(key) {
        var map = {
            "login": {"label": qsTr("Ingresar"), "icon": "🔑", "section": qsTr("Sistema")},
            "dashboard": {"label": qsTr("Tablero"), "icon": "📊", "section": qsTr("Principal")},
            "pos": {"label": qsTr("Punto de venta"), "icon": "🛒", "section": qsTr("Principal")},
            "products": {"label": qsTr("Productos"), "icon": "📦", "section": qsTr("Catálogo")},
            "sales": {"label": qsTr("Ventas"), "icon": "🧾", "section": qsTr("Ventas")},
            "clients": {"label": qsTr("Clientes"), "icon": "👥", "section": qsTr("Ventas")},
            "inventory": {"label": qsTr("Inventario"), "icon": "🏬", "section": qsTr("Catálogo")},
            "purchases": {"label": qsTr("Compras"), "icon": "🛍️", "section": qsTr("Catálogo")},
            "suppliers": {"label": qsTr("Proveedores"), "icon": "🚚", "section": qsTr("Catálogo")},
            "receivables": {"label": qsTr("Cuentas por cobrar"), "icon": "💳", "section": qsTr("Finanzas")},
            "payables": {"label": qsTr("Cuentas por pagar"), "icon": "💸", "section": qsTr("Finanzas")},
            "reports": {"label": qsTr("Reportes"), "icon": "📈", "section": qsTr("Finanzas")},
            "promos": {"label": qsTr("Promociones"), "icon": "🎟️", "section": qsTr("Ventas")},
            "users": {"label": qsTr("Usuarios"), "icon": "👤", "section": qsTr("Sistema")},
            "settings": {"label": qsTr("Configuración"), "icon": "⚙️", "section": qsTr("Sistema")},
            "lots": {"label": qsTr("Lotes y vencimientos"), "icon": "📅", "section": qsTr("Catálogo")},
            "serials": {"label": qsTr("Seriales y garantías"), "icon": "🔧", "section": qsTr("Catálogo")},
            "audit": {"label": qsTr("Bitácora"), "icon": "📜", "section": qsTr("Sistema")}
        };
        return map[key] || {"label": key, "icon": "•", "section": ""};
    }

    function crumbText() {
        var m = screenMeta(currentScreen);
        if (currentScreen === "login" || currentScreen === "dashboard")
            return m.icon + " " + m.label;
        return m.section + "  ›  " + m.icon + " " + m.label;
    }

    function navigate(screen) {
        if (!auth.canAccess(screen)) {
            if (!auth.loggedIn)
                screen = "login";
            else
                return; // rol sin permiso: no navegar (antes _snack)
        }
        currentScreen = screen;
        auth.touch(); // Fase 2: actividad contra expiración por inactividad
        if (screen === "dashboard")
            dash.refresh();
        if (screen === "sales")
            salesCtl.refresh();
        if (screen === "products")
            catalog.search("");
        stack.replace(pageFor(screen));
        drawer.close();
    }

    function pageFor(screen) {
        switch (screen) {
        case "login":
            return loginPage;
        case "dashboard":
            return dashboardPage;
        case "pos":
            return posPage;
        case "products":
            return productsPage;
        case "sales":
            return salesPage;
        case "clients":
            return clientsPage;
        case "suppliers":
            return suppliersPage;
        case "inventory":
            return inventoryPage;
        case "purchases":
            return purchasesPage;
        case "receivables":
            return receivablesPage;
        case "payables":
            return payablesPage;
        case "reports":
            return reportsPage;
        case "promos":
            return promosPage;
        case "users":
            return usersPage;
        case "settings":
            return settingsPage;
        case "lots":
            return lotsPage;
        case "serials":
            return serialsPage;
        case "audit":
            return auditPage;
        default:
            return dashboardPage;
        }
    }

    // Formato de moneda con el símbolo de settingsCtl (Fase 0 multinegocio).
    // Fase 7: agrupación localizada vía Qt.locale según settings["language"]
    // ("es" → es_CO con punto de miles, "en" → en_US con coma). Céntimos no se
    // muestran en UI (los montos viajan redondeados); el backend Money conserva
    // la precisión. Con fallback manual si Qt.locale falla.
    function money(v) {
        var sym = "$";
        var lang = "es";
        try {
            if (settingsCtl && settingsCtl.settings["currency_symbol"])
                sym = settingsCtl.settings["currency_symbol"];
            if (settingsCtl && settingsCtl.settings["language"])
                lang = settingsCtl.settings["language"];
        } catch (e) {}
        var n = Math.round(v);
        try {
            var loc = Qt.locale(lang === "en" ? "en_US" : "es_CO");
            var s = Number(n).toLocaleString(loc, 'f', 0);
            // toLocaleString conserva el signo; normalizar "-0" a "0".
            if (s === "-0" || s === "-0.0")
                s = "0";
            return sym + " " + s;
        } catch (e) {
            var neg = n < 0;
            n = Math.abs(n).toString();
            var out = "";
            while (n.length > 3) {
                out = "." + n.slice(-3) + out;
                n = n.slice(0, -3);
            }
            return (neg ? "-" + sym + " " : sym + " ") + n + out;
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4
            ToolButton {
                text: "\u2630"
                enabled: auth.loggedIn
                Accessible.name: qsTr("Abrir menú")
                onClicked: drawer.open()
            }
            // Breadcrumb clicable: ir al tablero
            ToolButton {
                visible: auth.loggedIn && currentScreen !== "dashboard" && currentScreen !== "login"
                text: qsTr("Tablero")
                Accessible.name: qsTr("Ir al tablero")
                onClicked: root.navigate("dashboard")
            }
            Label {
                visible: auth.loggedIn && currentScreen !== "dashboard" && currentScreen !== "login"
                text: "›"
                opacity: 0.6
            }
            Label {
                text: auth.loggedIn ? qsTr("Sistema de Ventas  ·  %1  ·  %2 (%3)").arg(crumbText()).arg(auth.currentUser).arg(auth.currentRole) : qsTr("Sistema de Ventas  ·  ") + crumbText()
                elide: Label.ElideRight
                Layout.fillWidth: true
            }
            // Fase 6: estado de sync (clic = sincronizar ahora).
            ToolButton {
                visible: auth.loggedIn
                text: pos.pendingSync > 0 ? qsTr("⏳ %1").arg(pos.pendingSync) : qsTr("🔄")
                Accessible.name: qsTr("Sincronizar pendientes")
                ToolTip.text: root.syncTip()
                ToolTip.visible: hovered
                onClicked: {
                    var r = syncSvc.syncNow(false);
                    Utils.showToast(r.failed > 0 ? "warning" : "success",
                        qsTr("Sincronizados %1 · fallos %2").arg(r.synced).arg(r.failed), 3000);
                    root.recheckOnline();
                }
            }
            // Reloj en vivo
            Label {
                visible: auth.loggedIn
                text: "🕒 " + clockText
                Accessible.name: qsTr("Fecha y hora actual")
            }
            // Estado de red (clic = re-chequear)
            ToolButton {
                visible: auth.loggedIn
                text: online ? qsTr("🟢") : qsTr("🔴")
                Accessible.name: online ? qsTr("En línea. Activar para comprobar conexión") : qsTr("Sin conexión. Activar para reintentar")
                ToolTip.text: online ? qsTr("En línea") : qsTr("Sin conexión — clic para reintentar")
                ToolTip.visible: hovered
                onClicked: {
                    var ok = recheckOnline();
                    Utils.showToast(ok ? "success" : "warning",
                        ok ? qsTr("Conexión disponible") : qsTr("Sin conexión: se trabaja offline"), 2500);
                }
            }
            // Alerta de stock bajo → inventario
            ToolButton {
                visible: auth.loggedIn && (dash.data.lowStockAlerts || 0) > 0
                text: qsTr("⚠️ %1").arg(dash.data.lowStockAlerts)
                Accessible.name: qsTr("Alerta de stock bajo. Ir a inventario")
                ToolTip.text: qsTr("Stock bajo — ir a inventario")
                ToolTip.visible: hovered
                onClicked: root.navigate("inventory")
            }
            ToolButton {
                visible: auth.loggedIn
                text: qsTr("Salir")
                onClicked: {
                    auth.logout();
                    root.navigate("login");
                }
            }
        }
    }

    Drawer {
        id: drawer
        width: 280
        height: parent.height
        AppSidebar {
            id: sidebar
            anchors.fill: parent
            currentKey: root.currentScreen
            onGo: screen => root.navigate(screen)
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        // Respiro general: todas las pantallas llevan margen superior y lateral.
        // Login no se altera (contenido centrado de ancho fijo).
        anchors.topMargin: Theme.spacingSmall
        anchors.leftMargin: Theme.marginMedium
        anchors.rightMargin: Theme.marginMedium
        initialItem: loginPage
    }
    
    // Global notification and loading overlays - outside StackView for proper visibility
    Toast {
        id: globalToastItem
        anchors.fill: parent
        z: Theme.zToast
    }

    LoadingOverlay {
        id: globalLoadingItem
        anchors.fill: parent
        z: Theme.zLoading
    }
    
    property alias globalToast: globalToastItem
    property alias globalLoading: globalLoadingItem

    LoginPage {
        id: loginPage
        visible: false
        onLoggedIn: root.navigate("dashboard")
    }
    DashboardPage {
        id: dashboardPage
        visible: false
        onGo: screen => root.navigate(screen)
    }
    PosPage {
        id: posPage
        visible: false
    }
    ProductsPage {
        id: productsPage
        visible: false
    }
    SalesPage {
        id: salesPage
        visible: false
        onGo: screen => root.navigate(screen)
    }
    ClientsPage {
        id: clientsPage
        visible: false
    }
    SuppliersPage {
        id: suppliersPage
        visible: false
    }
    InventoryPage {
        id: inventoryPage
        visible: false
    }
    PurchasesPage {
        id: purchasesPage
        visible: false
    }
    ReceivablesPage {
        id: receivablesPage
        visible: false
    }
    PayablesPage {
        id: payablesPage
        visible: false
    }
    ReportsPage {
        id: reportsPage
        visible: false
    }
    PromosPage {
        id: promosPage
        visible: false
    }
    UsersPage {
        id: usersPage
        visible: false
    }
    SettingsPage {
        id: settingsPage
        visible: false
    }
    LotsPage {
        id: lotsPage
        visible: false
    }
    SerialsPage {
        id: serialsPage
        visible: false
    }
    AuditPage {
        id: auditPage
        visible: false
    }

    // Atajos de teclado (mejora #10, personalizables en Fase 7 vía settings).
    // navigate() ya valida sesión y permiso por rol.
    Shortcut { sequence: root.shortcutSeq("shortcut_dashboard", "Ctrl+1"); enabled: auth.loggedIn; onActivated: root.navigate("dashboard") }
    Shortcut { sequence: root.shortcutSeq("shortcut_pos", "Ctrl+2"); enabled: auth.loggedIn; onActivated: root.navigate("pos") }
    Shortcut { sequence: root.shortcutSeq("shortcut_products", "Ctrl+3"); enabled: auth.loggedIn; onActivated: root.navigate("products") }
    Shortcut { sequence: root.shortcutSeq("shortcut_sales", "Ctrl+4"); enabled: auth.loggedIn; onActivated: root.navigate("sales") }
    Shortcut { sequence: root.shortcutSeq("shortcut_inventory", "Ctrl+5"); enabled: auth.loggedIn; onActivated: root.navigate("inventory") }
    Shortcut { sequence: root.shortcutSeq("shortcut_reports", "Ctrl+6"); enabled: auth.loggedIn; onActivated: root.navigate("reports") }
    Shortcut {
        sequence: root.shortcutSeq("shortcut_menu", "Ctrl+M"); enabled: auth.loggedIn
        onActivated: drawer.visible ? drawer.close() : drawer.open()
    }

    // Motores del header útil (mejora #12)
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.tickClock()
    }
    Timer {
        id: netTimer
        interval: 120000
        running: auth.loggedIn
        repeat: true
        onTriggered: root.recheckOnline()
    }
    // Fase 2: expiración por inactividad (30 min): cierra y regresa al login.
    Timer {
        id: idleTimer
        interval: 60000
        running: auth.loggedIn
        repeat: true
        onTriggered: {
            if (auth.checkIdle())
                root.navigate("login");
        }
    }
    Connections {
        target: auth
        function onSessionChanged() {
            if (auth.loggedIn) {
                root.tickClock();
                root.recheckOnline();
                settingsCtl.setRole(auth.currentRole);
                dash.refresh();
                sidebar.refresh();
            } else if (root.currentScreen !== "login") {
                root.navigate("login");
            }
        }
    }

    Connections {
        target: settingsCtl
        // Fase 4: el cambio de rubro re-filtra el menú por vertical.
        function onSettingsChanged() { sidebar.refresh(); }
    }
}
