// Proveedores (antes suppliers.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar proveedor…")
            Layout.fillWidth: true
            onAccepted: suppliersCtl.search(text)
        }
        Button {
            text: qsTr("Buscar")
            onClicked: suppliersCtl.search(searchField.text)
        }
        Button {
            text: qsTr("Nuevo")
            onClicked: {
                editDialog.supId = -1;
                editDialog.fields = {};
                editDialog.open();
            }
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: suppliersCtl.suppliers
        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData.name + "  ·  " + modelData.contact + "  ·  " + modelData.phone
            onClicked: {
                editDialog.supId = modelData.id;
                editDialog.fields = modelData;
                editDialog.open();
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Dialog {
        id: editDialog
        title: supId < 0 ? qsTr("Nuevo proveedor") : qsTr("Editar proveedor")
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        property int supId: -1
        property var fields: ({})
        ColumnLayout {
            TextField {
                id: fName
                text: editDialog.fields.name || ""
                placeholderText: qsTr("Empresa")
            }
            TextField {
                id: fContact
                text: editDialog.fields.contact || ""
                placeholderText: qsTr("Contacto")
            }
            TextField {
                id: fPhone
                text: editDialog.fields.phone || ""
                placeholderText: qsTr("Teléfono")
            }
            Label {
                id: editErr
                color: "red"
            }
        }
        onAccepted: {
            var f = {
                "name": fName.text,
                "contact": fContact.text,
                "phone": fPhone.text
            };
            var r = editDialog.supId < 0 ? suppliersCtl.add(f) : suppliersCtl.update(editDialog.supId, f);
            if (!r.ok) {
                editErr.text = r.error;
                open();
            }
        }
    }
}
