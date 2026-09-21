// Catálogo ABM (antes products.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar…")
            Layout.fillWidth: true
            onAccepted: catalog.search(text)
        }
        Button {
            text: qsTr("Buscar")
            onClicked: catalog.search(searchField.text)
        }
        Button {
            text: qsTr("Nuevo")
            onClicked: {
                editDialog.sku = "";
                editDialog.fields = {};
                editDialog.open();
            }
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: catalog.products
        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData.sku + "  ·  " + modelData.name + "  ·  " + money(modelData.price) + "  ·  stock " + modelData.stock
            onClicked: {
                editDialog.sku = modelData.sku;
                editDialog.fields = modelData;
                editDialog.open();
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Dialog {
        id: editDialog
        title: sku === "" ? qsTr("Nuevo producto") : qsTr("Editar ") + sku
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        property string sku: ""
        property var fields: ({})

        ColumnLayout {
            TextField {
                id: fName
                text: editDialog.fields.name || ""
                placeholderText: qsTr("Nombre")
            }
            RowLayout {
                TextField {
                    id: fPrice
                    text: editDialog.fields.price !== undefined ? editDialog.fields.price : ""
                    placeholderText: qsTr("Precio")
                }
                TextField {
                    id: fStock
                    text: editDialog.fields.stock !== undefined ? editDialog.fields.stock : ""
                    placeholderText: qsTr("Stock")
                }
            }
            TextField {
                id: fCat
                text: editDialog.fields.cat || ""
                placeholderText: qsTr("Categoría")
            }
            Label {
                id: editErr
                color: "red"
            }
        }
        onAccepted: {
            var fields = {
                "name": fName.text,
                "price": parseFloat(fPrice.text) || 0,
                "stock": parseInt(fStock.text) || 0,
                "cat": fCat.text || "General"
            };
            var r;
            if (editDialog.sku === "") {
                fields.sku = "P" + Date.now().toString().slice(-6);
                r = catalog.add(fields);
            } else {
                r = catalog.update(editDialog.sku, fields);
            }
            if (!r.ok) {
                editErr.text = r.error;
                open(); // reabrir si falló
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
