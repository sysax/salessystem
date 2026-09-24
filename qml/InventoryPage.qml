// Inventario: alertas, movimientos, ajustes, transferencias (antes inventory.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

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
        Button {
            text: qsTr("Merma")
            onClicked: wasteDialog.open()
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
        visible: (inventoryCtl.alerts.low || []).length > 0
        delegate: Label {
            width: ListView.view.width
            text: "⚠ " + modelData.sku + "  " + modelData.name + "  (stock " + modelData.stock + ")"
            color: Theme.error
        }
    }
    Label {
        visible: (inventoryCtl.alerts.low || []).length === 0
        text: qsTr("✅ Sin alertas: todo el stock está sobre el mínimo.")
        opacity: 0.7
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
        visible: (inventoryCtl.movements || []).length > 0
        delegate: Label {
            width: ListView.view.width
            text: modelData.ts + "  " + modelData.sku + "  " + modelData.type + "  " + (modelData.qty > 0 ? "+" : "") + Utils.formatQty(modelData.qty) + "  (" + Utils.formatQty(modelData.before) + "→" + Utils.formatQty(modelData.after) + ")"
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (inventoryCtl.movements || []).length === 0
        icon: "🏬"
        title: qsTr("Sin movimientos")
        hint: qsTr("Registra un ajuste o recibe mercancía en Compras.")
    }

    Dialog {
        id: adjustDialog
        onOpened: aSku.forceActiveFocus()
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
                color: Theme.error
            }
        }
        onAccepted: {
            var r = inventoryCtl.adjust(aSku.text, parseFloat(aQty.text) || 0, aReason.text, auth.currentUser);
            if (!r.ok) {
                aErr.text = r.error;
                open();
            }
        }
    }
    Dialog {
        id: transferDialog
        onOpened: tSku.forceActiveFocus()
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
                color: Theme.error
            }
        }
        onAccepted: {
            var r = inventoryCtl.transfer(tSku.text, parseFloat(tQty.text) || 0, tDest.text, tReason.text, auth.currentUser);
            if (!r.ok) {
                tErr.text = r.error;
                open();
            }
        }
    }
    Dialog {
        id: wasteDialog
        onOpened: wSku.forceActiveFocus()
        title: qsTr("Registrar merma")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: wSku
                placeholderText: qsTr("SKU")
            }
            TextField {
                id: wQty
                placeholderText: qsTr("Cantidad (admite decimales)")
                validator: DoubleValidator { bottom: 0.001; decimals: 3 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            TextField {
                id: wReason
                placeholderText: qsTr("Motivo (ej. vencido, roto)")
            }
            Label {
                id: wErr
                color: Theme.error
            }
        }
        onAccepted: {
            var r = inventoryCtl.waste(wSku.text, parseFloat(wQty.text) || 0, wReason.text, auth.currentUser);
            if (!r.ok) {
                wErr.text = r.error;
                open();
            } else {
                wSku.text = "";
                wQty.text = "";
                wReason.text = "";
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
