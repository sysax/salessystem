// Punto de venta: carrito, pagos mixtos, promos, ticket, caja (antes pos.py).
// Mejora #4 POS mejorado: botones táctiles 48px, confirmación al eliminar, swipe gestures.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem

RowLayout {
    id: root
    spacing: Theme.spacingMedium

    readonly property int touchH: 48   // objetivo táctil mínimo
    readonly property int rowHProduct: 56
    readonly property int rowHCart: 64

    function addProduct(productId) {
        var r = pos.addToCart(productId, 1);
        if (!r.ok)
            Utils.showToast("error", r.error, 3000);
        else
            Utils.showToast("success", "Agregado al carrito", 1500);
    }

    function askRemoveLine(idx, name) {
        confirmRemoveDialog.removeIndex = idx;
        confirmRemoveDialog.removeName = name || "";
        confirmRemoveDialog.open();
    }

    // Izquierda: búsqueda + carrito
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.spacingSmall

        RowLayout {
            spacing: Theme.spacingSmall
            TextField {
                id: searchField
                placeholderText: qsTr("Buscar producto (nombre, SKU, código)")
                Layout.fillWidth: true
                implicitHeight: root.touchH
                onAccepted: catalog.search(text)
            }
            Button {
                text: qsTr("Buscar")
                implicitHeight: root.touchH
                implicitWidth: 110
                onClicked: catalog.search(searchField.text)
            }
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            clip: true
            model: catalog.products
            delegate: SwipeDelegate {
                id: prodDelegate
                width: ListView.view.width
                height: root.rowHProduct
                text: modelData.name + "  ·  " + money(modelData.price) + "  ·  stock " + modelData.stock
                font.pixelSize: Theme.fontM
                onClicked: root.addProduct(modelData.id)
                // Deslizar para revelar acción táctil "Añadir"
                swipe.right: Button {
                    text: qsTr("Añadir +")
                    implicitHeight: root.rowHProduct
                    implicitWidth: 120
                    onClicked: {
                        prodDelegate.swipe.close();
                        root.addProduct(modelData.id);
                    }
                }
                swipe.onCompleted: {
                    // Swipe completo a la derecha también agrega
                    if (swipe.position > 0.5) {
                        swipe.close();
                        root.addProduct(modelData.id);
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        RowLayout {
            spacing: Theme.spacingSmall
            Label {
                text: qsTr("Carrito (%1)").arg(pos.cart.length)
                font.bold: true
                font.pixelSize: Theme.fontML
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Vaciar")
                implicitHeight: root.touchH
                visible: pos.cart.length > 0
                onClicked: confirmClearDialog.open()
            }
        }
        Label {
            visible: pos.cart.length === 0
            text: qsTr("Carrito vacío — toca un producto para agregar. Desliza un producto para añadir rápido.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            opacity: 0.7
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pos.cart
            delegate: SwipeDelegate {
                id: cartDelegate
                width: ListView.view.width
                height: root.rowHCart
                // Deslizar a la izquierda revela Eliminar
                swipe.left: Button {
                    text: qsTr("Eliminar")
                    implicitHeight: root.rowHCart
                    implicitWidth: 120
                    onClicked: {
                        cartDelegate.swipe.close();
                        root.askRemoveLine(index, modelData.name);
                    }
                }
                swipe.onCompleted: {
                    if (swipe.position < -0.5) {
                        swipe.close();
                        root.askRemoveLine(index, modelData.name);
                    }
                }
                contentItem: RowLayout {
                    spacing: Theme.spacingSmall
                    Label {
                        text: modelData.name + " × " + modelData.qty
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontM
                    }
                    Button {
                        text: "−"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontL
                        Accessible.name: qsTr("Disminuir cantidad")
                        onClicked: pos.setQty(index, modelData.qty - 1)
                        enabled: modelData.qty > 1
                    }
                    Button {
                        text: "+"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontL
                        Accessible.name: qsTr("Aumentar cantidad")
                        onClicked: pos.setQty(index, modelData.qty + 1)
                    }
                    Label {
                        text: money(modelData.subtotal)
                        Layout.preferredWidth: 110
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontM
                    }
                    Button {
                        text: "✕"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontML
                        Accessible.name: qsTr("Eliminar del carrito")
                        onClicked: root.askRemoveLine(index, modelData.name)
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    // Derecha: totales + cobro + caja
    ColumnLayout {
        Layout.preferredWidth: 320
        Layout.fillHeight: true
        spacing: Theme.spacingSmall

        GroupBox {
            title: qsTr("Totales")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 4
                Label {
                    text: qsTr("Subtotal: ") + money(pos.totals.subtotal)
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("Descuento: ") + money(pos.totals.discount)
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("IVA: ") + money(pos.totals.tax)
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("TOTAL: ") + money(pos.totals.total)
                    font.bold: true
                    font.pixelSize: Theme.fontXL
                }
            }
        }
        RowLayout {
            spacing: Theme.spacingSmall
            TextField {
                id: promoField
                placeholderText: qsTr("Código promo")
                Layout.fillWidth: true
                implicitHeight: root.touchH
            }
            Button {
                text: qsTr("Aplicar")
                implicitHeight: root.touchH
                implicitWidth: 100
                onClicked: {
                    var r = pos.applyPromo(promoField.text);
                    if (r.ok)
                        Utils.showToast("success", "Descuento: " + money(r.discount), 2500);
                    else
                        Utils.showToast("error", r.error, 3000);
                }
            }
        }
        ComboBox {
            id: methodBox
            Layout.fillWidth: true
            implicitHeight: root.touchH
            model: ["Efectivo", "Transferencia", "Tarjeta", "Credito", "Mixto"]
        }
        TextField {
            id: cashField
            placeholderText: qsTr("Efectivo recibido")
            Layout.fillWidth: true
            implicitHeight: root.touchH
            inputMethodHints: Qt.ImhDigitsOnly
            validator: DoubleValidator { bottom: 0 }
        }
        TextField {
            id: clientField
            placeholderText: qsTr("Cliente (opcional)")
            Layout.fillWidth: true
            implicitHeight: root.touchH
        }
        Button {
            text: qsTr("Cobrar")
            highlighted: true
            Layout.fillWidth: true
            implicitHeight: 56
            font.bold: true
            font.pixelSize: Theme.fontL
            enabled: pos.cart.length > 0
            onClicked: {
                var pays = {};
                if (methodBox.currentText === "Mixto" || methodBox.currentText === "Efectivo") {
                    var cash = parseFloat(cashField.text) || 0;
                    if (cash > 0)
                        pays["efectivo"] = cash;
                }
                Utils.showLoading(qsTr("Procesando venta..."));
                var r = pos.checkout(clientField.text, pays, methodBox.currentText, auth.currentUser);
                Utils.hideLoading();
                if (r.ok) {
                    Utils.showToast("success", "Venta " + r.saleId + " · Cambio " + money(r.change), 3000);
                    cashField.text = "";
                    promoField.text = "";
                } else {
                    Utils.showToast("error", r.error, 4000);
                }
            }
        }
        GroupBox {
            title: qsTr("Caja")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: pos.caja.open ? qsTr("Abierta · esperado %1").arg(money(pos.caja.expected)) : qsTr("Cerrada")
                    font.pixelSize: Theme.fontM
                }
                RowLayout {
                    spacing: Theme.spacingSmall
                    TextField {
                        id: cajaField
                        placeholderText: qsTr("Monto")
                        Layout.fillWidth: true
                        implicitHeight: root.touchH
                    }
                    Button {
                        text: pos.caja.open ? qsTr("Cerrar") : qsTr("Abrir")
                        implicitHeight: root.touchH
                        implicitWidth: 100
                        onClicked: {
                            var r = pos.caja.open ? pos.closeCaja(parseFloat(cajaField.text) || 0, auth.currentUser) : pos.openCaja(parseFloat(cajaField.text) || 0, auth.currentUser);
                            if (r.ok) {
                                if (r.diff !== undefined)
                                    Utils.showToast("warning", "Diferencia: " + money(r.diff), 3000);
                                else
                                    Utils.showToast("success", "Caja abierta", 2500);
                            } else {
                                Utils.showToast("error", r.error, 4000);
                            }
                        }
                    }
                }
            }
        }
    }

    // Confirmación antes de eliminar una línea (acción destructiva)
    Dialog {
        id: confirmRemoveDialog
        title: qsTr("Eliminar producto")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property int removeIndex: -1
        property string removeName: ""
        Label {
            text: qsTr("¿Eliminar \"%1\" del carrito?").arg(confirmRemoveDialog.removeName)
            wrapMode: Text.WordWrap
            width: 280
        }
        onAccepted: {
            if (removeIndex >= 0) {
                pos.removeLine(removeIndex);
                Utils.showToast("info", "Producto eliminado", 2000);
            }
            removeIndex = -1;
        }
        onRejected: removeIndex = -1
    }

    Dialog {
        id: confirmClearDialog
        title: qsTr("Vaciar carrito")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: qsTr("¿Vaciar todo el carrito? Esta acción no se puede deshacer.")
            wrapMode: Text.WordWrap
            width: 280
        }
        onAccepted: {
            pos.clearCart();
            Utils.showToast("info", "Carrito vaciado", 2000);
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
