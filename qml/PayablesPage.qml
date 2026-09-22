// Cuentas por pagar: pendientes, pagos, pronto pago (antes payables.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    Label {
        text: qsTr("Cuentas por pagar")
        font.pixelSize: Theme.fontL
        font.bold: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: cxpCtl.pending
        visible: (cxpCtl.pending || []).length > 0
        delegate: RowLayout {
            width: ListView.view.width
            Label {
                text: modelData.id + "  ·  " + modelData.supplier + "  ·  vence " + modelData.due + "  ·  saldo " + money(modelData.balance)
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Pagar")
                onClicked: {
                    payDialog.payId = modelData.id;
                    payDialog.open();
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (cxpCtl.pending || []).length === 0
        icon: "✅"
        title: qsTr("Sin cuentas por pagar")
        hint: qsTr("No hay facturas pendientes a proveedores.")
    }
    Label {
        id: msg
        color: Theme.error
    }

    Dialog {
        id: payDialog
        title: qsTr("Pagar ") + payId
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string payId: ""
        ColumnLayout {
            TextField {
                id: payAmount
                placeholderText: qsTr("Monto")
            }
        }
        onAccepted: {
            var r = cxpCtl.pay(payDialog.payId, parseFloat(payAmount.text) || 0, "Transferencia", auth.currentUser);
            msg.text = r.ok ? qsTr("Pagado. Descuento pronto pago: ") + money(r.earlyDiscount) : r.error;
            if (!r.ok)
                open();
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
