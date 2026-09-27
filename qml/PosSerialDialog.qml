// Diálogo de serial/IMEI para la línea recién agregada (extraído de PosPage).
// PosPage provee el stock vía openForLine() y ejecuta pos.setLineSerial al
// recibir serialChosen; ante error de backend llama showError() (reabre).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem

Dialog {
    id: root
    title: qsTr("Serial / IMEI")
    modal: true
    width: 320
    standardButtons: Dialog.Ok | Dialog.Cancel

    property int cartIndex: -1
    property int productId: -1
    // [{serial, imei2}] provisto por PosPage (pos.inStockSerials).
    property var serials: []

    signal serialChosen(string serial, int cartIndex)

    function openForLine(idx, pid, inStock) {
        cartIndex = idx;
        productId = pid;
        serialField.text = "";
        serialErr.text = "";
        serials = inStock || [];
        open();
    }
    // Error de backend (setLineSerial falló): muestra y reabre.
    function showError(msg) {
        serialErr.text = msg;
        open();
    }

    onOpened: serialField.forceActiveFocus()
    ColumnLayout {
        width: 280
        Label {
            text: qsTr("Escanee o seleccione el serial:")
            Layout.fillWidth: true
        }
        TextField {
            id: serialField
            placeholderText: qsTr("Serial / IMEI")
            Layout.fillWidth: true
        }
        ListView {
            id: serialList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(160, (count || 0) * 40)
            visible: (count || 0) > 0
            clip: true
            model: root.serials
            delegate: ItemDelegate {
                width: ListView.view.width
                text: modelData.serial + (modelData.imei2 ? " / " + modelData.imei2 : "")
                onClicked: serialField.text = modelData.serial
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Label {
            id: serialErr
            color: Theme.error
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
    onAccepted: {
        if (serialField.text.trim() === "") {
            serialErr.text = qsTr("Serial requerido para este producto.");
            open();
            return;
        }
        root.serialChosen(serialField.text, root.cartIndex);
    }
}
