// Catálogo ABM (antes products.py).
// Mejora #7: tabla funcional con ordenamiento, filtrado local y paginación.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    property string sortKey: "name"
    property bool sortAsc: true
    property int page: 0
    property int pageSize: 20
    property var viewRows: []
    property int totalRows: 0
    property int pageCount: 1

    RowLayout {
        TextField {
            id: searchField
            placeholderText: qsTr("Buscar… (servidor)")
            Layout.fillWidth: true
            implicitHeight: 48
            onAccepted: { root.page = 0; catalog.search(text); }
        }
        Button {
            text: qsTr("Buscar")
            implicitHeight: 48
            onClicked: { root.page = 0; catalog.search(searchField.text); }
        }
        Button {
            text: qsTr("Nuevo")
            implicitHeight: 48
            onClicked: {
                editDialog.sku = "";
                editDialog.fields = {};
                editErr.text = "";
                editDialog.open();
            }
        }
    }
    RowLayout {
        TextField {
            id: filterField
            placeholderText: qsTr("Filtrar en vista (nombre, SKU, categoría)…")
            Layout.fillWidth: true
            implicitHeight: 40
            onTextChanged: { root.page = 0; root.refreshView(); }
        }
        Label {
            text: qsTr("%1 ítems").arg(root.totalRows)
            opacity: 0.7
        }
    }
    // Encabezados ordenables
    RowLayout {
        spacing: Theme.spacingSmall
        SortHeader {
            label: qsTr("SKU")
            active: root.sortKey === "sku"
            asc: root.sortAsc
            Layout.preferredWidth: 120
            onClicked: root.setSort("sku")
        }
        SortHeader {
            label: qsTr("Nombre")
            active: root.sortKey === "name"
            asc: root.sortAsc
            Layout.fillWidth: true
            onClicked: root.setSort("name")
        }
        SortHeader {
            label: qsTr("Precio")
            active: root.sortKey === "price"
            asc: root.sortAsc
            Layout.preferredWidth: 110
            onClicked: root.setSort("price")
        }
        SortHeader {
            label: qsTr("Stock")
            active: root.sortKey === "stock"
            asc: root.sortAsc
            Layout.preferredWidth: 90
            onClicked: root.setSort("stock")
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
                editDialog.sku = modelData.sku;
                editDialog.fields = modelData;
                editDialog.open();
            }
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: modelData.sku
                    Layout.preferredWidth: 120
                    elide: Text.ElideRight
                }
                Label {
                    text: modelData.name
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: money(modelData.price)
                    Layout.preferredWidth: 110
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: String(modelData.stock)
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                    color: modelData.stock <= 0 ? "red" : palette.text
                    font.bold: modelData.stock <= 0
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.viewRows.length === 0
        icon: "📦"
        title: (catalog.products || []).length === 0 ? qsTr("Sin productos") : qsTr("Sin resultados")
        hint: (catalog.products || []).length === 0 ? qsTr("Crea el primero con “Nuevo” o importa desde Compras.") : qsTr("Ajusta el filtro o la búsqueda.")
        actionText: (catalog.products || []).length === 0 ? qsTr("Nuevo producto") : ""
        onAction: {
            editDialog.sku = "";
            editDialog.fields = {};
            editDialog.open();
        }
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
            root.sortAsc = true;
        }
        root.page = 0;
        root.refreshView();
    }

    function valOf(m, key) {
        if (key === "sku")
            return String(m.sku || "");
        if (key === "name")
            return String(m.name || "");
        if (key === "price")
            return Number(m.price) || 0;
        return Number(m.stock) || 0;
    }

    function refreshView() {
        var f = filterField.text.toLowerCase();
        var base = [];
        var src = catalog.products || [];
        for (var i = 0; i < src.length; ++i) {
            var m = src[i];
            if (f !== "") {
                var hay = String(m.sku || "").toLowerCase() + " " + String(m.name || "").toLowerCase() + " " + String(m.cat || "").toLowerCase();
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
        target: catalog
        function onProductsChanged() { root.refreshView(); }
    }

    Dialog {
        id: editDialog
        onOpened: fName.forceActiveFocus()
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
                    validator: DoubleValidator { bottom: 0 }
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
                TextField {
                    id: fStock
                    text: editDialog.fields.stock !== undefined ? editDialog.fields.stock : ""
                    placeholderText: qsTr("Stock")
                    validator: IntValidator { bottom: 0; top: 9999999 }
                    inputMethodHints: Qt.ImhDigitsOnly
                }
            }
            TextField {
                id: fCat
                text: editDialog.fields.cat || ""
                placeholderText: qsTr("Categoría")
            }
            Label {
                id: editErr
                color: Theme.error
            }
            Label {
                // Ayuda reactiva: primer problema del formulario (validación en vivo)
                text: fName.text.trim() === "" ? qsTr("Ingrese el nombre") :
                      !fPrice.acceptableInput ? qsTr("Precio inválido (≥ 0)") :
                      !fStock.acceptableInput ? qsTr("Stock inválido (entero ≥ 0)") : ""
                color: Theme.error
                visible: text !== ""
            }
        }
        Component.onCompleted: {
            // Guardar solo con formulario válido (el chequeo backend en onAccepted se conserva)
            editDialog.standardButton(Dialog.Save).enabled = Qt.binding(function() {
                return fName.text.trim() !== "" && fPrice.acceptableInput && fStock.acceptableInput;
            });
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
            } else {
                Utils.showToast("success", editDialog.sku === "" ? "Producto creado" : "Producto actualizado", 2500);
                close();
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
