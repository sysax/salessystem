// Promociones ABM (antes promos.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall
    property string pendingRemoveId: ""
    property string pendingRemoveCode: ""

    RowLayout {
        Label {
            text: qsTr("Promociones y descuentos")
            font.pixelSize: Theme.fontL
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
        font.pixelSize: Theme.fontXS
        opacity: 0.7
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: promosCtl.promos
        visible: (promosCtl.promos || []).length > 0
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
                onClicked: {
                    root.pendingRemoveId = modelData.id;
                    root.pendingRemoveCode = modelData.code;
                    confirmRemovePromo.open();
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (promosCtl.promos || []).length === 0
        icon: "🎟️"
        title: qsTr("Sin promociones")
        hint: qsTr("Crea la primera con “Nueva”: porcentaje, 2x1, cupón, happy hour…")
        actionText: qsTr("Nueva promoción")
        onAction: addDialog.open()
    }

    Dialog {
        id: addDialog
        onOpened: pCode.forceActiveFocus()
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
                color: Theme.error
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

    ConfirmDialog {
        id: confirmRemovePromo
        title: qsTr("Eliminar promoción")
        message: qsTr("¿Eliminar la promoción %1? Dejará de aplicarse en el POS.").arg(root.pendingRemoveCode)
        confirmText: qsTr("Sí, eliminar")
        onAccepted: {
            promosCtl.remove(root.pendingRemoveId);
            Utils.showToast("info", qsTr("Promoción eliminada"), 2000);
            root.pendingRemoveId = "";
            root.pendingRemoveCode = "";
        }
        onRejected: {
            root.pendingRemoveId = "";
            root.pendingRemoveCode = "";
        }
    }
}
