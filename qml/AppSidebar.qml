// Menú lateral filtrado por rol (antes components/sidebar.py).
// Claves de pantalla = permisos de ROLE_PERMISSIONS.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    signal go(string screen)

    property var entries: [
        { "key": "dashboard", "label": "Tablero" },
        { "key": "pos", "label": "Punto de venta" },
        { "key": "products", "label": "Productos" },
        { "key": "sales", "label": "Ventas" },
        { "key": "clients", "label": "Clientes" },
        { "key": "inventory", "label": "Inventario" },
        { "key": "purchases", "label": "Compras" },
        { "key": "suppliers", "label": "Proveedores" },
        { "key": "receivables", "label": "Cuentas por cobrar" },
        { "key": "payables", "label": "Cuentas por pagar" },
        { "key": "reports", "label": "Reportes" },
        { "key": "promos", "label": "Promociones" },
        { "key": "users", "label": "Usuarios" },
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
            text: modelData.label
            highlighted: ListView.view.currentIndex === index
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
    }

    Component.onCompleted: refresh()

    Connections {
        target: auth
        // Sintaxis onSignal: clásica, válida en todas las versiones (function onX requiere Qt nuevo)
        onSessionChanged: root.refresh()
    }
}
