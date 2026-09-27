// Diálogo de cantidad decimal para productos a granel (extraído de PosPage).
// Solo valida y emite; PosPage ejecuta pos.addToCart/setQty y confirma con
// confirmDone() o reporta error de backend con showError() (reabre).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

Dialog {
    id: root
    title: qsTr("Cantidad")
    modal: true
    width: 320
    standardButtons: Dialog.Ok | Dialog.Cancel

    property int productId: -1
    property int cartIndex: -1
    property string productName: ""

    signal qtyChosen(double qty, int cartIndex, int productId)

    // Agregar producto pesable (cantidad inicial "1").
    function openForProduct(pid, name) {
        productId = pid;
        cartIndex = -1;
        productName = name || "";
        qtyField.text = "1";
        qtyErr.text = "";
        open();
    }
    // Editar cantidad exacta de una línea pesable del carrito.
    function openForLine(idx, name, qtyText) {
        productId = -1;
        cartIndex = idx;
        productName = name || "";
        qtyField.text = qtyText || "";
        qtyErr.text = "";
        open();
    }
    // Error de backend (p.ej. addToCart falló): muestra y reabre sin perder
    // el modo (se conserva cartIndex/productId para reintentar).
    function showError(msg) {
        qtyErr.text = msg;
        open();
    }
    // Éxito de backend: limpia el modo.
    function confirmDone() {
        cartIndex = -1;
        productId = -1;
    }

    onOpened: qtyField.forceActiveFocus()
    ColumnLayout {
        width: 280
        Label {
            text: root.productName
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.bold: true
        }
        // Fase 2: qty pesable admite 3 decimales (>0); precio solo 2 (backend Money).
        MoneyField {
            id: qtyField
            placeholderText: qsTr("Cantidad (ej. 0.350)")
            label: qsTr("Cantidad (ej. 0.350)")
            Layout.fillWidth: true
            maxValue: 999999
            allowNegative: false
            maxDecimals: 3
            onErrorTextChanged: qtyErr.text = errorText
        }
        Label {
            id: qtyErr
            color: Theme.error
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
    onAccepted: {
        if (!qtyField.isValid()) {
            qtyErr.text = qtyField.errorText || qsTr("Cantidad inválida.");
            open();
            return;
        }
        var q = qtyField.parsedValue();
        if (!(q > 0)) {
            qtyErr.text = qsTr("Cantidad mayor a 0.");
            open();
            return;
        }
        root.qtyChosen(q, root.cartIndex, root.productId);
    }
    onRejected: {
        root.cartIndex = -1;
        root.productId = -1;
    }
}
