// Cuentas por cobrar: saldos, abonos, mora (antes receivables.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    Label {
        text: qsTr("Cuentas por cobrar")
        font.pixelSize: 18
        font.bold: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: cxcCtl.pending
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
    Label {
        id: msg
        color: "red"
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
