// Compras OC → recepción → CxP (antes purchases.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        Label {
            text: qsTr("Órdenes de compra")
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Nueva OC")
            onClicked: createDialog.open()
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: purchasesCtl.orders
        visible: (purchasesCtl.orders || []).length > 0
        delegate: RowLayout {
            width: ListView.view.width
            Label {
                text: modelData.id + "  ·  " + modelData.supplier + "  ·  " + money(modelData.total) + "  ·  " + modelData.status
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Recibir")
                enabled: modelData.status === "Pendiente"
                onClicked: {
                    var r = purchasesCtl.receive(modelData.id, auth.currentUser);
                    if (!r.ok)
                        msg.text = r.error;
                }
            }
            Button {
                text: qsTr("Cancelar")
                enabled: modelData.status === "Pendiente"
                onClicked: {
                    var r = purchasesCtl.cancel(modelData.id, auth.currentUser);
                    if (!r.ok)
                        msg.text = r.error;
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (purchasesCtl.orders || []).length === 0
        icon: "🛍️"
        title: qsTr("Sin órdenes de compra")
        hint: qsTr("Crea la primera con “Nueva OC”; al recibirla se actualiza inventario y CxP.")
        actionText: qsTr("Nueva OC")
        onAction: createDialog.open()
    }
    Label {
        id: msg
        color: "red"
    }

    Dialog {
        id: createDialog
        title: qsTr("Nueva orden de compra")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: cSupplier
                placeholderText: qsTr("Proveedor (nombre exacto)")
            }
            TextField {
                id: cSku
                placeholderText: qsTr("SKU")
            }
            TextField {
                id: cQty
                placeholderText: qsTr("Cantidad")
            }
            Label {
                id: cErr
                color: "red"
            }
        }
        onAccepted: {
            var r = purchasesCtl.create(cSupplier.text, cSku.text, parseInt(cQty.text) || 0, auth.currentUser);
            if (!r.ok) {
                cErr.text = r.error;
                open();
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
