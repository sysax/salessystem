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

    Material.theme: Material.Light
    Material.primary: Theme.primary   // menta pastel
    Material.accent: Theme.accent

    property string currentScreen: "login"

    // Header útil (mejora #12): reloj, estado de red y alertas.
    property string clockText: "--/-- --:--:--"
    property bool online: true

    function tickClock() {
        clockText = Qt.formatDateTime(new Date(), "dd/MM HH:mm:ss");
    }

    function recheckOnline() {
        // isOnline() bloquea hasta 1.5s solo sin red; por eso se llama al login,
        // cada 120s y manualmente, nunca en cada navegación.
        online = syncSvc.isOnline();
        return online;
    }

    // Mejora #5: metadatos para breadcrumbs + sidebar (etiqueta, icono, sección).
    function screenMeta(key) {
        var map = {
            "login": {"label": "Ingresar", "icon": "🔑", "section": "Sistema"},
            "dashboard": {"label": "Tablero", "icon": "📊", "section": "Principal"},
            "pos": {"label": "Punto de venta", "icon": "🛒", "section": "Principal"},
            "products": {"label": "Productos", "icon": "📦", "section": "Catálogo"},
            "sales": {"label": "Ventas", "icon": "🧾", "section": "Ventas"},
            "clients": {"label": "Clientes", "icon": "👥", "section": "Ventas"},
            "inventory": {"label": "Inventario", "icon": "🏬", "section": "Catálogo"},
            "purchases": {"label": "Compras", "icon": "🛍️", "section": "Catálogo"},
            "suppliers": {"label": "Proveedores", "icon": "🚚", "section": "Catálogo"},
            "receivables": {"label": "Cuentas por cobrar", "icon": "💳", "section": "Finanzas"},
            "payables": {"label": "Cuentas por pagar", "icon": "💸", "section": "Finanzas"},
            "reports": {"label": "Reportes", "icon": "📈", "section": "Finanzas"},
            "promos": {"label": "Promociones", "icon": "🎟️", "section": "Ventas"},
            "users": {"label": "Usuarios", "icon": "👤", "section": "Sistema"},
            "settings": {"label": "Configuración", "icon": "⚙️", "section": "Sistema"}
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
        default:
            return dashboardPage;
        }
    }

    // Formato de moneda con el símbolo de settingsCtl (Fase 0 multinegocio).
    function money(v) {
        var sym = "$ ";
        try {
            if (settingsCtl && settingsCtl.settings["currency_symbol"])
                sym = settingsCtl.settings["currency_symbol"] + " ";
        } catch (e) {}
        var n = Math.round(v);
        var neg = n < 0;
        n = Math.abs(n).toString();
        var out = "";
        while (n.length > 3) {
            out = "." + n.slice(-3) + out;
            n = n.slice(0, -3);
        }
        return (neg ? "-" + sym : sym) + n + out;
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
                text: qsTr("Sistema de Ventas  ·  ") + crumbText() + (auth.loggedIn ? "  ·  " + auth.currentUser + " (" + auth.currentRole + ")" : "")
                elide: Label.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: pos.pendingSync > 0
                text: qsTr("⏳ %1 por sincronizar").arg(pos.pendingSync)
                color: Material.color(Material.Orange)
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

    // Atajos de teclado (mejora #10): navegación rápida entre módulos principales.
    // navigate() ya valida sesión y permiso por rol.
    Shortcut { sequence: "Ctrl+1"; enabled: auth.loggedIn; onActivated: root.navigate("dashboard") }
    Shortcut { sequence: "Ctrl+2"; enabled: auth.loggedIn; onActivated: root.navigate("pos") }
    Shortcut { sequence: "Ctrl+3"; enabled: auth.loggedIn; onActivated: root.navigate("products") }
    Shortcut { sequence: "Ctrl+4"; enabled: auth.loggedIn; onActivated: root.navigate("sales") }
    Shortcut { sequence: "Ctrl+5"; enabled: auth.loggedIn; onActivated: root.navigate("inventory") }
    Shortcut { sequence: "Ctrl+6"; enabled: auth.loggedIn; onActivated: root.navigate("reports") }
    Shortcut {
        sequence: "Ctrl+M"; enabled: auth.loggedIn
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
    Connections {
        target: auth
        function onSessionChanged() {
            if (auth.loggedIn) {
                root.tickClock();
                root.recheckOnline();
                settingsCtl.setRole(auth.currentRole);
                dash.refresh();
            }
        }
    }
}
