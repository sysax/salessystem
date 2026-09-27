// Clientes CRM + estado de cuenta + abonos (antes clients.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    property bool loading: false
    property string pendingOp: "search"

    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar cliente…")
            Layout.fillWidth: true
            onAccepted: root.requestSearch()
        }
        Button {
            text: qsTr("Buscar")
            onClicked: root.requestSearch()
        }
        BusyIndicator {
            visible: root.loading
            running: root.loading
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
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
        id: clientList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: clientsCtl.clientModel
        delegate: ItemDelegate {
            width: ListView.view.width
            text: model.name + "  ·  " + model.balance + " saldo  ·  " + model.status
            onClicked: {
                stmtModel.model = clientsCtl.statement(model.name);
                stmtLabel.text = qsTr("Estado: ") + model.name;
            }
            onPressAndHold: {
                editDialog.clientId = model.id;
                editDialog.fields = clientsCtl.clientModel.get(index);
                editDialog.open();
            }
        }
        ScrollBar.vertical: ScrollBar {}
        // Fase 4: scroll infinito (anexa el siguiente lote al llegar abajo).
        onMovementEnded: {
            if (clientsCtl.clientModel.canFetchMore && !root.loading) {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
        }
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: clientList.count === 0 && !root.loading
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
    // Fase 4: pie incremental (en lugar del Pager discreto).
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSmall
        Label {
            text: qsTr("Mostrando %1 de %2").arg(clientList.count).arg(clientsCtl.clientModel.totalCount)
            opacity: 0.7
            Layout.fillWidth: true
        }
        BusyIndicator {
            visible: root.loading && clientList.count > 0
            running: root.loading && clientList.count > 0
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
        }
        Button {
            text: qsTr("Cargar más")
            visible: clientsCtl.clientModel.canFetchMore
            onClicked: {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
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
        onOpened: fName.forceActiveFocus()
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
            // Fase 2: límite de crédito >= 0, 2 decimales (Money backend).
            MoneyField {
                id: fCreditLimit
                text: editDialog.fields.creditLimit !== undefined ? editDialog.fields.creditLimit : ""
                placeholderText: qsTr("Límite de crédito")
                label: qsTr("Límite de crédito")
                Layout.fillWidth: true
                maxValue: 999999999
                allowNegative: false
                maxDecimals: 2
            }
            Label {
                id: editErr
                color: Theme.error
            }
            Label {
                text: fName.text.trim() === "" ? qsTr("Ingrese el nombre") :
                      !fPhone.acceptableInput ? qsTr("Teléfono inválido") :
                      !fCreditLimit.acceptableInput ? (fCreditLimit.errorText || qsTr("Límite inválido (≥ 0)")) : ""
                color: Theme.error
                visible: text !== ""
            }
        }
        Component.onCompleted: {
            editDialog.standardButton(Dialog.Save).enabled = Qt.binding(function() {
                return fName.text.trim() !== "" && fPhone.acceptableInput && fCreditLimit.acceptableInput;
            });
        }
        onAccepted: {
            if (!fCreditLimit.isValid()) {
                editErr.text = fCreditLimit.errorText || qsTr("Límite de crédito inválido");
                open();
                return;
            }
            var f = {
                "name": fName.text,
                "nit": fNit.text,
                "phone": fPhone.text,
                "city": fCity.text,
                "creditLimit": fCreditLimit.text.trim() !== "" ? fCreditLimit.amount() : 0
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
        onOpened: payAmount.forceActiveFocus()
        title: qsTr("Abonar ") + saleId
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string saleId: ""
        ColumnLayout {
            // Fase 2: abono >= 0.01, 2 decimales.
            MoneyField {
                id: payAmount
                placeholderText: qsTr("Monto")
                label: qsTr("Monto")
                Layout.fillWidth: true
                maxValue: 999999999
                allowNegative: false
                maxDecimals: 2
            }
            Label {
                visible: payAmount.errorText !== ""
                text: payAmount.errorText
                color: Theme.error
            }
        }
        onAccepted: {
            if (!payAmount.isValid()) {
                payAmount.text = "";
                open();
                return;
            }
            var r = clientsCtl.pay(payDialog.saleId, payAmount.amount(), "Efectivo", auth.currentUser);
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

    // Fase 4: scroll infinito (búsqueda al modelo; lotes de 30).
    function requestSearch() {
        root.pendingOp = "search";
        root.loading = true;
        queryTimer.restart();
    }
    function doSearch() {
        if (root.pendingOp === "more") {
            clientsCtl.fetchMoreClients();
        } else {
            clientsCtl.searchClients(searchField.text);
        }
        root.loading = false;
    }

    Timer {
        id: queryTimer
        interval: 5
        repeat: false
        onTriggered: root.doSearch()
    }
    Component.onCompleted: root.requestSearch()
    onVisibleChanged: if (visible) root.requestSearch()
}
