// Compras OC → recepción → CxP (antes purchases.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall
    property string pendingCancelOc: ""

    RowLayout {
        Label {
            text: qsTr("Órdenes de compra")
            font.pixelSize: Theme.fontL
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
                    root.pendingCancelOc = modelData.id;
                    confirmCancelOc.open();
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
        color: Theme.error
    }

    ConfirmDialog {
        id: confirmCancelOc
        title: qsTr("Cancelar orden")
        message: qsTr("¿Cancelar la orden %1? No se puede deshacer.").arg(root.pendingCancelOc)
        confirmText: qsTr("Sí, cancelar")
        onAccepted: {
            var r = purchasesCtl.cancel(root.pendingCancelOc, auth.currentUser);
            if (!r.ok)
                msg.text = r.error;
            root.pendingCancelOc = "";
        }
        onRejected: root.pendingCancelOc = ""
    }

    Dialog {
        id: createDialog
        onOpened: cSupplier.forceActiveFocus()
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
                color: Theme.error
            }
        }
        onAccepted: {
            var r = purchasesCtl.create(cSupplier.text, cSku.text, parseFloat(cQty.text) || 0, auth.currentUser);
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
