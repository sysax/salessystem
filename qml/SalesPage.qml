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
    property int page: 0
    property int pageSize: 20
    property var viewRows: []
    property int totalRows: 0
    property int pageCount: 1

    RowLayout {
        Label {
            text: qsTr("Ventas y documentos")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Actualizar")
            implicitHeight: 48
            onClicked: salesCtl.refresh()
        }
    }
    RowLayout {
        TextField {
            id: filterField
            placeholderText: qsTr("Filtrar (cliente, folio, estado, documento)…")
            Layout.fillWidth: true
            implicitHeight: 40
            onTextChanged: { root.page = 0; root.refreshView(); }
        }
        Label {
            text: qsTr("%1 ítems").arg(root.totalRows)
            opacity: 0.7
        }
    }
    // Fase 3: verificación de garantía / RMA por serial.
    GroupBox {
        title: qsTr("Garantía / RMA")
        Layout.fillWidth: true
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
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.viewRows
        visible: root.viewRows.length > 0
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 48
            onClicked: {
                var d = salesCtl.detail(modelData.id);
                detailText.text = d.ok ? JSON.stringify(d, null, 1) : d.error;
                detailDialog.saleId = modelData.id;
                detailDialog.open();
            }
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: modelData.id
                    Layout.preferredWidth: 100
                    elide: Text.ElideRight
                }
                Label {
                    text: modelData.client
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: money(modelData.total)
                    Layout.preferredWidth: 110
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: modelData.status
                    Layout.preferredWidth: 110
                    elide: Text.ElideRight
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.viewRows.length === 0
        icon: "🧾"
        title: (salesCtl.sales || []).length === 0 ? qsTr("Sin ventas aún") : qsTr("Sin resultados")
        hint: (salesCtl.sales || []).length === 0 ? qsTr("Las ventas cobradas en el POS aparecerán aquí.") : qsTr("Ajusta el filtro.")
        actionText: (salesCtl.sales || []).length === 0 ? qsTr("Ir al POS") : ""
        onAction: root.go("pos")
    }
    Pager {
        Layout.fillWidth: true
        page: root.page
        pageCount: root.pageCount
        total: root.totalRows
        pageSize: root.pageSize
        onFirst: { root.page = 0; root.refreshView(); }
        onPrev: { if (root.page > 0) { root.page--; root.refreshView(); } }
        onNext: { if (root.page < root.pageCount - 1) { root.page++; root.refreshView(); } }
        onLast: { root.page = root.pageCount - 1; root.refreshView(); }
        onSizeChanged: function(size) { root.pageSize = size; root.page = 0; root.refreshView(); }
    }

    function setSort(key) {
        if (root.sortKey === key)
            root.sortAsc = !root.sortAsc;
        else {
            root.sortKey = key;
            root.sortAsc = key === "client" || key === "status";
        }
        root.page = 0;
        root.refreshView();
    }

    function valOf(m, key) {
        if (key === "client")
            return String(m.client || "");
        if (key === "status")
            return String(m.status || "");
        if (key === "total")
            return Number(m.total) || 0;
        return String(m.id || "");
    }

    function refreshView() {
        var f = filterField.text.toLowerCase();
        var base = [];
        var src = salesCtl.sales || [];
        for (var i = 0; i < src.length; ++i) {
            var m = src[i];
            if (f !== "") {
                var hay = String(m.id || "").toLowerCase() + " " + String(m.client || "").toLowerCase() + " " + String(m.status || "").toLowerCase() + " " + String(m.docType || "").toLowerCase();
                if (hay.indexOf(f) < 0)
                    continue;
            }
            base.push(m);
        }
        var k = root.sortKey;
        var asc = root.sortAsc;
        base.sort(function(a, b) {
            var va = root.valOf(a, k);
            var vb = root.valOf(b, k);
            if (va < vb)
                return asc ? -1 : 1;
            if (va > vb)
                return asc ? 1 : -1;
            return 0;
        });
        root.totalRows = base.length;
        root.pageCount = Math.max(1, Math.ceil(base.length / root.pageSize));
        if (root.page >= root.pageCount)
            root.page = root.pageCount - 1;
        if (root.page < 0)
            root.page = 0;
        var out = [];
        var start = root.page * root.pageSize;
        var end = Math.min(start + root.pageSize, base.length);
        for (var j = start; j < end; ++j)
            out.push(base[j]);
        root.viewRows = out;
    }

    Component.onCompleted: refreshView()

    Connections {
        target: salesCtl
        function onSalesChanged() { root.refreshView(); }
    }

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
            var r = salesCtl.cancel(detailDialog.saleId, "UI", auth.currentUser);
            if (!r.ok)
                detailText.text = r.error;
            else
                detailDialog.close();
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }

    function checkWarranty() {
        var r = salesCtl.warrantyFor(warrantyField.text.trim());
        if (!r.ok)
            warrantyMsg.text = r.error;
        else
            warrantyMsg.text = (r.detail || "") + " · " + (r.serial || "");
    }
}
