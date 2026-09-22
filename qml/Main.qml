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
    }

    Material.theme: Material.Light
    Material.primary: Theme.primary   // menta pastel
    Material.accent: Theme.accent

    property string currentScreen: "login"

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
        default:
            return dashboardPage;
        }
    }

    // Formato COP "$ 1.850.000"
    function money(v) {
        var n = Math.round(v);
        var neg = n < 0;
        n = Math.abs(n).toString();
        var out = "";
        while (n.length > 3) {
            out = "." + n.slice(-3) + out;
            n = n.slice(0, -3);
        }
        return (neg ? "-$ " : "$ ") + n + out;
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "\u2630"
                enabled: auth.loggedIn
                onClicked: drawer.open()
            }
            Label {
                text: qsTr("Sistema de Ventas  ·  ") + currentScreen + (auth.loggedIn ? "  ·  " + auth.currentUser + " (" + auth.currentRole + ")" : "")
                elide: Label.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: pos.pendingSync > 0
                text: qsTr("⏳ %1 por sincronizar").arg(pos.pendingSync)
                color: Material.color(Material.Orange)
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
        width: 260
        height: parent.height
        AppSidebar {
            anchors.fill: parent
            onGo: screen => root.navigate(screen)
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
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
}
