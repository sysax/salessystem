// Punto de venta: carrito, pagos mixtos, promos, ticket, caja (antes pos.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 12

    // Izquierda: búsqueda + carrito
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 8

        RowLayout {
            TextField {
                id: searchField
                placeholderText: qsTr("Buscar producto (nombre, SKU, código)")
                Layout.fillWidth: true
                onAccepted: catalog.search(text)
            }
            Button {
                text: qsTr("Buscar")
                onClicked: catalog.search(searchField.text)
            }
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            clip: true
            model: catalog.products
            delegate: ItemDelegate {
                width: ListView.view.width
                text: modelData.name + "  ·  " + money(modelData.price) + "  ·  stock " + modelData.stock
                onClicked: {
                    var r = pos.addToCart(modelData.id, 1);
                    if (!r.ok)
                        msg.text = r.error;
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Label {
            text: qsTr("Carrito")
            font.bold: true
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pos.cart
            delegate: RowLayout {
                width: ListView.view.width
                Label {
                    text: modelData.name + " × " + modelData.qty
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Button {
                    text: "−"
                    onClicked: pos.setQty(index, modelData.qty - 1)
                    enabled: modelData.qty > 1
                }
                Button {
                    text: "+"
                    onClicked: pos.setQty(index, modelData.qty + 1)
                }
                Label {
                    text: money(modelData.subtotal)
                    Layout.preferredWidth: 110
                    horizontalAlignment: Text.AlignRight
                }
                Button {
                    text: "✕"
                    onClicked: pos.removeLine(index)
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    // Derecha: totales + cobro + caja
    ColumnLayout {
        Layout.preferredWidth: 300
        Layout.fillHeight: true
        spacing: 8

        GroupBox {
            title: qsTr("Totales")
            Layout.fillWidth: true
            ColumnLayout {
                Label {
                    text: qsTr("Subtotal: ") + money(pos.totals.subtotal)
                }
                Label {
                    text: qsTr("Descuento: ") + money(pos.totals.discount)
                }
                Label {
                    text: qsTr("IVA: ") + money(pos.totals.tax)
                }
                Label {
                    text: qsTr("TOTAL: ") + money(pos.totals.total)
                    font.bold: true
                    font.pixelSize: 18
                }
            }
        }
        RowLayout {
            TextField {
                id: promoField
                placeholderText: qsTr("Código promo")
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Aplicar")
                onClicked: {
                    var r = pos.applyPromo(promoField.text);
                    msg.text = r.ok ? qsTr("Promo: ") + money(r.discount) : r.error;
                }
            }
        }
        ComboBox {
            id: methodBox
            Layout.fillWidth: true
            model: ["Efectivo", "Transferencia", "Tarjeta", "Credito", "Mixto"]
        }
        TextField {
            id: cashField
            placeholderText: qsTr("Efectivo recibido")
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhDigitsOnly
        }
        TextField {
            id: clientField
            placeholderText: qsTr("Cliente (opcional)")
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Cobrar")
            highlighted: true
            Layout.fillWidth: true
            enabled: pos.cart.length > 0
            onClicked: {
                var pays = {};
                if (methodBox.currentText === "Mixto" || methodBox.currentText === "Efectivo") {
                    var cash = parseFloat(cashField.text) || 0;
                    if (cash > 0)
                        pays["efectivo"] = cash;
                }
                var r = pos.checkout(clientField.text, pays, methodBox.currentText, auth.currentUser);
                if (r.ok) {
                    msg.text = qsTr("Venta %1 · Cambio %2 · %3").arg(r.saleId).arg(money(r.change)).arg(r.ticket);
                    cashField.text = "";
                    promoField.text = "";
                } else {
                    msg.text = r.error;
                }
            }
        }
        Label {
            id: msg
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        GroupBox {
            title: qsTr("Caja")
            Layout.fillWidth: true
            ColumnLayout {
                Label {
                    text: pos.caja.open ? qsTr("Abierta · esperado %1").arg(money(pos.caja.expected)) : qsTr("Cerrada")
                }
                RowLayout {
                    TextField {
                        id: cajaField
                        placeholderText: qsTr("Monto")
                        Layout.fillWidth: true
                    }
                    Button {
                        text: pos.caja.open ? qsTr("Cerrar") : qsTr("Abrir")
                        onClicked: {
                            var r = pos.caja.open ? pos.closeCaja(parseFloat(cajaField.text) || 0, auth.currentUser) : pos.openCaja(parseFloat(cajaField.text) || 0, auth.currentUser);
                            msg.text = r.ok ? (r.diff !== undefined ? qsTr("Diferencia: ") + money(r.diff) : qsTr("Caja abierta")) : r.error;
                        }
                    }
                }
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
