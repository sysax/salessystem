// Historial y documentos (antes sales.py).
// Mejora #7: tabla funcional con ordenamiento, filtrado local y paginación.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall
    signal go(string screen)

    property string sortKey: "id"
    property bool sortAsc: false
    property bool loading: false
    property string pendingOp: "search"

    RowLayout {
        Label {
            text: qsTr("Ventas y documentos")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        BusyIndicator {
            visible: root.loading
            running: root.loading
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
        }
        Button {
            text: qsTr("Actualizar")
            implicitHeight: 48
            onClicked: root.requestView()
        }
    }
    RowLayout {
        TextField {
            id: filterField
            placeholderText: qsTr("Filtrar en servidor (folio, cliente, estado)…")
            Layout.fillWidth: true
            implicitHeight: 40
            onTextChanged: root.requestView()
        }
        Label {
            text: qsTr("%1 de %2").arg(salesList.count).arg(salesCtl.saleModel.totalCount)
            opacity: 0.7
        }
    }
    // Fase 3: verificación de garantía / RMA por serial.
    // Multitienda: solo en rubros con seriales (celulares, taller...).
    GroupBox {
        title: qsTr("Garantía / RMA")
        Layout.fillWidth: true
        visible: root.supportsSerial()
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingSmall
            RowLayout {
                TextField {
                    id: warrantyField
                    placeholderText: qsTr("Serial / IMEI")
                    Layout.fillWidth: true
                    onAccepted: root.checkWarranty()
                }
                Button {
                    text: qsTr("Verificar")
                    onClicked: root.checkWarranty()
                }
                Button {
                    text: qsTr("Pasar a RMA")
                    enabled: warrantyMsg.text !== "" && auth.currentRole === "Administrador"
                    onClicked: {
                        var r = salesCtl.markRma(warrantyField.text.trim(), "RMA desde ventas", auth.currentUser);
                        warrantyMsg.text = r.ok ? qsTr("Serial en RMA.") : r.error;
                    }
                }
            }
            Label {
                id: warrantyMsg
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }
    RowLayout {
        spacing: Theme.spacingSmall
        SortHeader {
            label: qsTr("Folio")
            active: root.sortKey === "id"
            asc: root.sortAsc
            Layout.preferredWidth: 100
            onClicked: root.setSort("id")
        }
        SortHeader {
            label: qsTr("Cliente")
            active: root.sortKey === "client"
            asc: root.sortAsc
            Layout.fillWidth: true
            onClicked: root.setSort("client")
        }
        SortHeader {
            label: qsTr("Total")
            active: root.sortKey === "total"
            asc: root.sortAsc
            Layout.preferredWidth: 110
            onClicked: root.setSort("total")
        }
        SortHeader {
            label: qsTr("Estado")
            active: root.sortKey === "status"
            asc: root.sortAsc
            Layout.preferredWidth: 110
            onClicked: root.setSort("status")
        }
    }
    ListView {
        id: salesList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: salesCtl.saleModel
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 48
            onClicked: {
                var d = salesCtl.detail(model.id);
                detailText.text = d.ok ? JSON.stringify(d, null, 1) : d.error;
                detailDialog.saleId = model.id;
                detailDialog.open();
            }
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: model.id
                    Layout.preferredWidth: 100
                    elide: Text.ElideRight
                }
                Label {
                    text: model.client
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: money(model.total)
                    Layout.preferredWidth: 110
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: model.status
                    Layout.preferredWidth: 110
                    elide: Text.ElideRight
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
        // Fase 4: scroll infinito (anexa el siguiente lote al llegar abajo).
        onMovementEnded: {
            if (salesCtl.saleModel.canFetchMore && !root.loading) {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
        }
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: salesList.count === 0 && !root.loading
        icon: "🧾"
        title: salesCtl.saleModel.totalCount === 0 ? qsTr("Sin ventas aún") : qsTr("Sin resultados")
        hint: salesCtl.saleModel.totalCount === 0 ? qsTr("Las ventas cobradas en el POS aparecerán aquí.") : qsTr("Ajusta el filtro.")
        actionText: salesCtl.saleModel.totalCount === 0 ? qsTr("Ir al POS") : ""
        onAction: root.go("pos")
    }
    // Fase 4: pie incremental (en lugar del Pager discreto).
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSmall
        Label {
            text: qsTr("Mostrando %1 de %2").arg(salesList.count).arg(salesCtl.saleModel.totalCount)
            opacity: 0.7
            Layout.fillWidth: true
        }
        BusyIndicator {
            visible: root.loading && salesList.count > 0
            running: root.loading && salesList.count > 0
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
        }
        Button {
            text: qsTr("Cargar más")
            visible: salesCtl.saleModel.canFetchMore
            onClicked: {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
        }
    }

    function setSort(key) {
        if (root.sortKey === key)
            root.sortAsc = !root.sortAsc;
        else {
            root.sortKey = key;
            root.sortAsc = key === "client" || key === "status";
        }
        root.requestView();
    }

    // Fase 4: scroll infinito (filtro + orden al modelo; lotes de 30).
    function requestView() {
        root.pendingOp = "search";
        root.loading = true;
        queryTimer.restart();
    }
    function doView() {
        if (root.pendingOp === "more") {
            salesCtl.fetchMoreSales();
        } else {
            salesCtl.searchSales(filterField.text, root.sortKey, root.sortAsc);
        }
        root.loading = false;
    }

    Timer {
        id: queryTimer
        interval: 5
        repeat: false
        onTriggered: root.doView()
    }

    Component.onCompleted: root.requestView()
    onVisibleChanged: if (visible) root.requestView()

    Dialog {
        id: detailDialog
        title: qsTr("Detalle ") + saleId
        modal: true
        standardButtons: Dialog.Close
        property string saleId: ""
        ColumnLayout {
            ScrollView {
                Layout.preferredWidth: Math.min(420, (ApplicationWindow.window ? ApplicationWindow.window.width : 480) - 64)
                Layout.preferredHeight: Math.min(240, (ApplicationWindow.window ? ApplicationWindow.window.height : 640) - 320)
                TextArea {
                    id: detailText
                    readOnly: true
                    wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                Button {
                    text: qsTr("Avanzar a Pagada")
                    onClicked: {
                        var r = salesCtl.advance(detailDialog.saleId, "Pagada", auth.currentUser);
                        if (!r.ok)
                            detailText.text = r.error;
                        else
                            detailDialog.close();
                    }
                }
                Button {
                    text: qsTr("Cancelar venta")
                    // Fase 3: anular exige supervisor (el backend lo valida también).
                    visible: auth.currentRole === "Administrador"
                    onClicked: confirmCancelSale.open()
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmCancelSale
        title: qsTr("Cancelar venta")
        message: qsTr("¿Cancelar la venta %1? Se reversará el stock y no se puede deshacer.").arg(detailDialog.saleId)
        confirmText: qsTr("Sí, cancelar")
        onAccepted: {
            var r = salesCtl.cancel(detailDialog.saleId, "UI", auth.currentUser, auth.currentRole);
            if (!r.ok)
                detailText.text = r.error;
            else
                detailDialog.close();
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
    // Multitienda: RMA/garantía solo en rubros con seriales.
    function businessType() {
        try { return settingsCtl.settings["business_type"] || "miscelanea"; } catch (e) { return "miscelanea"; }
    }
    function supportsSerial() {
        var bt = businessType();
        return bt === "miscelanea" || bt === "celulares" || bt === "taller" || bt === "ferreteria";
    }

    function checkWarranty() {
        var r = salesCtl.warrantyFor(warrantyField.text.trim());
        if (!r.ok)
            warrantyMsg.text = r.error;
        else
            warrantyMsg.text = (r.detail || "") + " · " + (r.serial || "");
    }
}
