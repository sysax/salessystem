// Inventario: alertas, movimientos, ajustes, transferencias (antes inventory.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        Label {
            text: qsTr("Valorización: ") + money(inventoryCtl.valuation().totalValue)
            Layout.fillWidth: true
            font.bold: true
        }
        Button {
            text: qsTr("Actualizar")
            onClicked: inventoryCtl.refresh()
        }
        Button {
            text: qsTr("Ajuste")
            onClicked: adjustDialog.open()
        }
        Button {
            text: qsTr("Transferir")
            onClicked: transferDialog.open()
        }
    }
    Label {
        text: qsTr("Stock bajo / agotados")
        font.bold: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: 110
        clip: true
        model: inventoryCtl.alerts.low || []
        delegate: Label {
            width: ListView.view.width
            text: "⚠ " + modelData.sku + "  " + modelData.name + "  (stock " + modelData.stock + ")"
            color: "red"
        }
    }
    Label {
        text: qsTr("Movimientos recientes")
        font.bold: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: inventoryCtl.movements
        delegate: Label {
            width: ListView.view.width
            text: modelData.ts + "  " + modelData.sku + "  " + modelData.type + "  " + (modelData.qty > 0 ? "+" : "") + modelData.qty + "  (" + modelData.before + "→" + modelData.after + ")"
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Dialog {
        id: adjustDialog
        title: qsTr("Ajuste de stock (requiere motivo)")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: aSku
                placeholderText: qsTr("SKU")
            }
            TextField {
                id: aQty
                placeholderText: qsTr("Cantidad (+/−)")
            }
            TextField {
                id: aReason
                placeholderText: qsTr("Motivo (obligatorio)")
            }
            Label {
                id: aErr
                color: "red"
            }
        }
        onAccepted: {
            var r = inventoryCtl.adjust(aSku.text, parseInt(aQty.text) || 0, aReason.text, auth.currentUser);
            if (!r.ok) {
                aErr.text = r.error;
                open();
            }
        }
    }
    Dialog {
        id: transferDialog
        title: qsTr("Transferir ubicación")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: tSku
                placeholderText: qsTr("SKU")
            }
            TextField {
                id: tQty
                placeholderText: qsTr("Cantidad")
            }
            TextField {
                id: tDest
                placeholderText: qsTr("Ubicación destino")
            }
            TextField {
                id: tReason
                placeholderText: qsTr("Motivo")
            }
            Label {
                id: tErr
                color: "red"
            }
        }
        onAccepted: {
            var r = inventoryCtl.transfer(tSku.text, parseInt(tQty.text) || 0, tDest.text, tReason.text, auth.currentUser);
            if (!r.ok) {
                tErr.text = r.error;
                open();
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
