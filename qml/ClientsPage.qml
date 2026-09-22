// Clientes CRM + estado de cuenta + abonos (antes clients.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar cliente…")
            Layout.fillWidth: true
            onAccepted: clientsCtl.search(text)
        }
        Button {
            text: qsTr("Buscar")
            onClicked: clientsCtl.search(searchField.text)
        }
        Button {
            text: qsTr("Nuevo")
            onClicked: {
                editDialog.clientId = -1;
                editDialog.fields = {};
                editDialog.open();
            }
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: clientsCtl.clients
        visible: (clientsCtl.clients || []).length > 0
        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData.name + "  ·  " + modelData.balance + " saldo  ·  " + modelData.status
            onClicked: {
                stmtModel.model = clientsCtl.statement(modelData.name);
                stmtLabel.text = qsTr("Estado: ") + modelData.name;
            }
            onPressAndHold: {
                editDialog.clientId = modelData.id;
                editDialog.fields = modelData;
                editDialog.open();
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: (clientsCtl.clients || []).length === 0
        icon: "👥"
        title: qsTr("Sin clientes")
        hint: qsTr("Registra el primero con “Nuevo” para asignar ventas y crédito.")
        actionText: qsTr("Nuevo cliente")
        onAction: {
            editDialog.clientId = -1;
            editDialog.fields = {};
            editDialog.open();
        }
    }
    Label {
        id: stmtLabel
        text: qsTr("Toque un cliente para ver su estado de cuenta")
        font.bold: true
    }
    ListView {
        id: stmtModel
        Layout.fillWidth: true
        Layout.preferredHeight: 140
        clip: true
        delegate: RowLayout {
            width: stmtModel.width
            Label {
                text: modelData.id + "  " + modelData.date
                Layout.fillWidth: true
            }
            Label {
                text: money(modelData.balance)
            }
            Button {
                text: qsTr("Abonar")
                onClicked: {
                    payDialog.saleId = modelData.id;
                    payDialog.open();
                }
            }
        }
    }

    Dialog {
        id: editDialog
        title: clientId < 0 ? qsTr("Nuevo cliente") : qsTr("Editar cliente")
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        property int clientId: -1
        property var fields: ({})
        ColumnLayout {
            TextField {
                id: fName
                text: editDialog.fields.name || ""
                placeholderText: qsTr("Nombre")
            }
            TextField {
                id: fNit
                text: editDialog.fields.nit || ""
                placeholderText: qsTr("NIT")
            }
            TextField {
                id: fPhone
                text: editDialog.fields.phone || ""
                placeholderText: qsTr("Teléfono")
                validator: RegularExpressionValidator { regularExpression: /[0-9+\-\s]*/ }
                inputMethodHints: Qt.ImhDialableCharactersOnly
            }
            TextField {
                id: fCity
                text: editDialog.fields.city || ""
                placeholderText: qsTr("Ciudad")
            }
            Label {
                id: editErr
                color: Theme.error
            }
            Label {
                text: fName.text.trim() === "" ? qsTr("Ingrese el nombre") :
                      !fPhone.acceptableInput ? qsTr("Teléfono inválido") : ""
                color: Theme.error
                visible: text !== ""
            }
        }
        Component.onCompleted: {
            editDialog.standardButton(Dialog.Save).enabled = Qt.binding(function() {
                return fName.text.trim() !== "" && fPhone.acceptableInput;
            });
        }
        onAccepted: {
            var f = {
                "name": fName.text,
                "nit": fNit.text,
                "phone": fPhone.text,
                "city": fCity.text
            };
            var r = editDialog.clientId < 0 ? clientsCtl.add(f) : clientsCtl.update(editDialog.clientId, f);
            if (!r.ok) {
                editErr.text = r.error;
                open();
            }
        }
    }
    Dialog {
        id: payDialog
        title: qsTr("Abonar ") + saleId
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string saleId: ""
        ColumnLayout {
            TextField {
                id: payAmount
                placeholderText: qsTr("Monto")
                validator: DoubleValidator { bottom: 0.01 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }
        onAccepted: {
            var r = clientsCtl.pay(payDialog.saleId, parseFloat(payAmount.text) || 0, "Efectivo", auth.currentUser);
            if (!r.ok) {
                payAmount.text = "";
                open();
            }
        }
        Component.onCompleted: {
            payDialog.standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return payAmount.acceptableInput;
            });
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
