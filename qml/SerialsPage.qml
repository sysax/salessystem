// Fase 4: Seriales y garantías (celulares, taller).
// Registro, listado por estado, garantía y RMA.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    property string statusFilter: ""

    RowLayout {
        Label {
            text: qsTr("Seriales y garantías")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        ComboBox {
            id: statusBox
            model: ["Todos", "in_stock", "sold", "rma", "repaired"]
            onCurrentTextChanged: {
                root.statusFilter = (currentText === "Todos") ? "" : currentText;
                serialsCtl.search(searchField.text, root.statusFilter);
            }
        }
        Button {
            text: qsTr("Nuevo")
            onClicked: addDialog.open()
        }
    }
    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar serial o SKU…")
            Layout.fillWidth: true
            implicitHeight: 40
            onAccepted: serialsCtl.search(text, root.statusFilter)
        }
        Button {
            text: qsTr("Buscar")
            implicitHeight: 40
            onClicked: serialsCtl.search(searchField.text, root.statusFilter)
        }
    }
    Label {
        id: msg
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        color: Theme.error
        visible: text !== ""
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: serialsCtl.serials
        visible: (serialsCtl.serials || []).length > 0
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 56
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: modelData.serial + "  ·  " + (modelData.product || modelData.sku)
                        font.pixelSize: Theme.fontM
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: modelData.status + (modelData.saleId ? "  ·  " + modelData.saleId : "")
                        font.pixelSize: Theme.fontS
                        opacity: 0.7
                    }
                }
                Button {
                    text: qsTr("Garantía")
                    visible: modelData.status === "sold"
                    onClicked: {
                        var r = serialsCtl.warrantyFor(modelData.serial);
                        msg.color = Theme.info;
                        msg.text = r.ok ? (r.detail + " · " + r.serial) : r.error;
                    }
                }
                Button {
                    text: qsTr("RMA")
                    visible: modelData.status === "sold" || modelData.status === "repaired"
                    onClicked: {
                        rmaSerialField.text = modelData.serial;
                        rmaDialog.open();
                    }
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (serialsCtl.serials || []).length === 0
        icon: "🔧"
        title: qsTr("Sin seriales")
        hint: qsTr("Registra el primero con “Nuevo” o ajusta el filtro.")
        actionText: qsTr("Nuevo serial")
        onAction: addDialog.open()
    }

    Dialog {
        id: addDialog
        title: qsTr("Registrar serial")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string sku: ""
        onOpened: sSku.forceActiveFocus()
        ColumnLayout {
            width: 280
            TextField {
                id: sSku
                placeholderText: qsTr("SKU del producto")
                Layout.fillWidth: true
            }
            TextField {
                id: sSerial
                placeholderText: qsTr("Serial / IMEI")
                Layout.fillWidth: true
            }
            TextField {
                id: sImei2
                placeholderText: qsTr("IMEI2 (opcional)")
                Layout.fillWidth: true
            }
            Label {
                id: sErr
                color: Theme.error
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            var r = serialsCtl.addSerial(sSku.text, sSerial.text, sImei2.text);
            if (!r.ok) {
                sErr.text = r.error;
                open();
            } else {
                sSku.text = "";
                sSerial.text = "";
                sImei2.text = "";
                sErr.text = "";
            }
        }
    }

    Dialog {
        id: rmaDialog
        title: qsTr("Pasar a RMA")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string serial: ""
        ColumnLayout {
            width: 280
            TextField {
                id: rmaSerialField
                readOnly: true
                Layout.fillWidth: true
            }
            TextField {
                id: rmaNotes
                placeholderText: qsTr("Motivo / notas")
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            var r = serialsCtl.setStatus(rmaSerialField.text, "rma", rmaNotes.text, auth.currentUser);
            msg.color = r.ok ? Theme.success : Theme.error;
            msg.text = r.ok ? qsTr("Serial en RMA.") : r.error;
            rmaNotes.text = "";
        }
    }

    Component.onCompleted: serialsCtl.search("", "")
    Connections {
        target: serialsCtl
        function onSerialsChanged() {}
    }
}
