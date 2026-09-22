// Cuentas por cobrar: saldos, abonos, mora (antes receivables.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    Label {
        text: qsTr("Cuentas por cobrar")
        font.pixelSize: Theme.fontL
        font.bold: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: cxcCtl.pending
        visible: (cxcCtl.pending || []).length > 0
        delegate: RowLayout {
            width: ListView.view.width
            Label {
                text: modelData.id + "  ·  " + modelData.client + "  ·  saldo " + money(modelData.balance) + "  ·  mora " + money(modelData.mora)
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Abonar")
                onClicked: {
                    payDialog.saleId = modelData.id;
                    payDialog.open();
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (cxcCtl.pending || []).length === 0
        icon: "✅"
        title: qsTr("Sin cuentas por cobrar")
        hint: qsTr("No hay saldos pendientes: todo cobrado. Las ventas a crédito aparecerán aquí.")
    }
    Label {
        id: msg
        color: Theme.error
    }

    Dialog {
        id: payDialog
        title: qsTr("Abonar ") + saleId
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string saleId: ""
        ColumnLayout {
            TextField {
                id: payAmount
                placeholderText: qsTr("Monto")
            }
        }
        onAccepted: {
            var r = cxcCtl.pay(payDialog.saleId, parseFloat(payAmount.text) || 0, "Efectivo", auth.currentUser);
            msg.text = r.ok ? qsTr("Abono registrado, saldo: ") + money(r.balance) : r.error;
            if (!r.ok)
                open();
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
