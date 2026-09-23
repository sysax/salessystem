// Menú lateral filtrado por rol (antes components/sidebar.py).
// Claves de pantalla = permisos de ROLE_PERMISSIONS.
// Mejora #5: iconos, indicador de pantalla activa, objetivo táctil 48px.
// Nota: sin import QtSalesSystem a propósito — tst_sidebar carga este archivo
// aislado sin el módulo registrado; los valores igualan Theme (8/14/16/18).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    signal go(string screen)

    property string currentKey: "dashboard"

    property var entries: [
        { "key": "dashboard", "label": "Tablero", "icon": "📊", "sc": "Ctrl+1" },
        { "key": "pos", "label": "Punto de venta", "icon": "🛒", "sc": "Ctrl+2" },
        { "key": "products", "label": "Productos", "icon": "📦", "sc": "Ctrl+3" },
        { "key": "sales", "label": "Ventas", "icon": "🧾", "sc": "Ctrl+4" },
        { "key": "clients", "label": "Clientes", "icon": "👥", "sc": "" },
        { "key": "inventory", "label": "Inventario", "icon": "🏬", "sc": "Ctrl+5" },
        { "key": "purchases", "label": "Compras", "icon": "🛍️", "sc": "" },
        { "key": "suppliers", "label": "Proveedores", "icon": "🚚", "sc": "" },
        { "key": "receivables", "label": "Cuentas por cobrar", "icon": "💳", "sc": "" },
        { "key": "payables", "label": "Cuentas por pagar", "icon": "💸", "sc": "" },
        { "key": "reports", "label": "Reportes", "icon": "📈", "sc": "Ctrl+6" },
        { "key": "promos", "label": "Promociones", "icon": "🎟️", "sc": "" },
        { "key": "users", "label": "Usuarios", "icon": "👤", "sc": "" },
        { "key": "settings", "label": "Configuración", "icon": "⚙️", "sc": "" },
    ]

    Label {
        text: qsTr("Menú")
        font.pixelSize: 18
        font.bold: true
        Layout.margins: 12
    }
    ListView {
        id: menuList
        objectName: "menuList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 48
            highlighted: modelData.key === root.currentKey
            Accessible.name: modelData.label
            contentItem: RowLayout {
                spacing: 8
                // Barra de pantalla activa
                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.fillHeight: true
                    radius: 2
                    color: modelData.key === root.currentKey ? "#5AA9E6" : "transparent"
                }
                Label {
                    text: modelData.icon
                    font.pixelSize: 16
                    Layout.preferredWidth: 28
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    text: modelData.label
                    font.pixelSize: 14
                    font.bold: modelData.key === root.currentKey
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    visible: modelData.sc !== ""
                    text: modelData.sc
                    font.pixelSize: 12
                    opacity: 0.6
                }
            }
            onClicked: root.go(modelData.key)
        }
    }

    function refresh() {
        // Bucle clásico: las arrow functions no cargan en Qt 6.4 (CI)
        var visible = [];
        for (var i = 0; i < root.entries.length; ++i) {
            if (auth.canAccess(root.entries[i].key))
                visible.push(root.entries[i]);
        }
        menuList.model = visible;
        syncCurrent();
    }

    function syncCurrent() {
        // Mantiene currentIndex alineado con currentKey para el resaltado por teclado
        if (!menuList.model)
            return;
        for (var i = 0; i < menuList.model.length; ++i) {
            if (menuList.model[i].key === root.currentKey) {
                menuList.currentIndex = i;
                return;
            }
        }
    }

    onCurrentKeyChanged: syncCurrent()

    Component.onCompleted: refresh()

    Connections {
        target: auth
        function onSessionChanged() { root.refresh(); }
    }
}
