// Promociones ABM (antes promos.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        Label {
            text: qsTr("Promociones y descuentos")
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Nueva")
            onClicked: {
                addDialog.open();
            }
        }
    }
    Label {
        text: qsTr("Tipos: porcentaje · monto_fijo · 2x1 · 3x2 · volumen · cupon · happy_hour")
        font.pixelSize: 11
        opacity: 0.7
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: promosCtl.promos
        delegate: RowLayout {
            width: ListView.view.width
            CheckBox {
                checked: modelData.active
                onClicked: promosCtl.setActive(modelData.id, checked)
            }
            Label {
                text: modelData.code + "  ·  " + modelData.name + "  ·  " + modelData.type + " " + modelData.value
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Eliminar")
                onClicked: promosCtl.remove(modelData.id)
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Dialog {
        id: addDialog
        title: qsTr("Nueva promoción")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: pCode
                placeholderText: qsTr("Código (ej. ELEC10)")
            }
            TextField {
                id: pName
                placeholderText: qsTr("Nombre")
            }
            ComboBox {
                id: pType
                model: ["porcentaje", "monto_fijo", "2x1", "3x2", "volumen", "cupon", "happy_hour"]
            }
            TextField {
                id: pValue
                placeholderText: qsTr("Valor")
            }
            TextField {
                id: pCond
                placeholderText: qsTr("Condición (categoría, SKU o min N)")
            }
            Label {
                id: pErr
                color: "red"
            }
        }
        onAccepted: {
            var r = promosCtl.add({
                "code": pCode.text,
                "name": pName.text,
                "type": pType.currentText,
                "value": parseFloat(pValue.text) || 0,
                "condition": pCond.text,
                "active": true
            });
            if (!r.ok) {
                pErr.text = r.error;
                open();
            }
        }
    }
}
