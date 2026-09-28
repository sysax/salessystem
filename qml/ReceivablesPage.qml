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
                text: qsTr("%1  ·  %2  ·  saldo %3  ·  mora %4").arg(modelData.id).arg(modelData.client).arg(money(modelData.balance)).arg(money(modelData.mora))
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
        onOpened: payAmount.forceActiveFocus()
        title: qsTr("Abonar ") + saleId
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string saleId: ""
        ColumnLayout {
            // Fase 2: abono >= 0, 2 decimales (MoneyField + Money backend).
            MoneyField {
                id: payAmount
                placeholderText: qsTr("Monto")
                label: qsTr("Monto")
                Layout.fillWidth: true
                maxValue: 999999999
                allowNegative: false
                maxDecimals: 2
            }
            Label {
                visible: payAmount.errorText !== ""
                text: payAmount.errorText
                color: Theme.error
            }
        }
        onAccepted: {
            if (!payAmount.isValid()) {
                open();
                return;
            }
            var r = cxcCtl.pay(payDialog.saleId, payAmount.amount(), "Efectivo", auth.currentUser);
            msg.text = r.ok ? qsTr("Abono registrado, saldo: ") + money(r.balance) : r.error;
            if (!r.ok)
                open();
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
