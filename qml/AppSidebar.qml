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
        menuList.model = root.entries.filter(e => auth.canAccess(e.key));
    }

    Component.onCompleted: refresh()

    Connections {
        target: auth
        function onSessionChanged() {
            root.refresh();
        }
    }
}
