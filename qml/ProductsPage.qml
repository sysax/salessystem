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

    // Fase 1: nombres de tasas desde Configuración (sin SQL en QML).
    function taxNames() {
        try {
            var raw = settingsCtl.settings["tax_rates_json"] || "[]";
            var arr = JSON.parse(raw);
            var out = [];
            for (var i = 0; i < arr.length; ++i) {
                if (arr[i].name)
                    out.push(arr[i].name);
            }
            return out.length > 0 ? out : ["IVA 19%", "Excluido"];
        } catch (e) {
            return ["IVA 19%", "Excluido"];
        }
    }
    function defaultTaxName() {
        try {
            var d = parseFloat(settingsCtl.settings["default_tax_rate"]);
            var arr = JSON.parse(settingsCtl.settings["tax_rates_json"] || "[]");
            for (var i = 0; i < arr.length; ++i) {
                if (parseFloat(arr[i].rate) === d)
                    return arr[i].name;
            }
        } catch (e) {}
        return "IVA 19%";
    }

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
                    text: Utils.formatQty(modelData.stock) + (modelData.unit ? " " + modelData.unit : "")
                    Layout.preferredWidth: 110
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
        hint: (catalog.products || []).length === 0 ? qsTr("Crea el primero con “Nuevo”, importa desde Compras o inicializa el catálogo del rubro en Configuración.") : qsTr("Ajusta el filtro o la búsqueda.")
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

    // Fase 2: categorías desde el diccionario (sin SQL en QML).
    function rootCatNames() {
        var out = [];
        var cats = catalog.categories || [];
        for (var i = 0; i < cats.length; ++i) {
            if ((cats[i].parentId || 0) === 0)
                out.push(cats[i].name);
        }
        return out;
    }
    function catIdByName(name) {
        var cats = catalog.categories || [];
        for (var i = 0; i < cats.length; ++i) {
            if (cats[i].name === name && ((cats[i].parentId || 0) === 0))
                return cats[i].id;
        }
        return 0;
    }
    function subcatNames(parentId) {
        var out = [];
        if (!parentId)
            return out;
        var cats = catalog.categories || [];
        for (var i = 0; i < cats.length; ++i) {
            if ((cats[i].parentId || 0) === parentId)
                out.push(cats[i].name);
        }
        return out;
    }
    function reloadCats() {
        var bt = "";
        try { bt = settingsCtl.settings["business_type"] || ""; } catch (e) {}
        catalog.reloadCategories(bt);
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

    Component.onCompleted: { refreshView(); reloadCats(); }

    Connections {
        target: catalog
        function onProductsChanged() { root.refreshView(); }
        function onCategoriesChanged() { root.refreshView(); }
    }
    Connections {
        target: settingsCtl
        function onSettingsChanged() { root.reloadCats(); }
    }

    Dialog {
        id: editDialog
        onOpened: {
            fName.forceActiveFocus();
            // Preseleccionar impuesto actual (o default al crear)
            var cur = editDialog.fields.tax || defaultTaxName();
            var names = taxNames();
            var idx = names.indexOf(cur);
            if (idx >= 0) {
                fTax.currentIndex = idx;
            } else {
                fTax.editText = cur;
            }
            // Fase 2: preseleccionar categoría/subcategoría/unidad
            var cn = editDialog.fields.cat || "";
            var ci = rootCatNames().indexOf(cn);
            if (ci >= 0) {
                fCat.currentIndex = ci;
            } else if (cn !== "") {
                fCat.editText = cn;
            }
            var sid = catIdByName(fCat.editText !== "" ? fCat.editText : fCat.currentText);
            fSubcat.model = subcatNames(sid);
            var sn = editDialog.fields.subcat || "";
            var si = fSubcat.model.indexOf(sn);
            if (si >= 0)
                fSubcat.currentIndex = si;
            else if (sn !== "")
                fSubcat.editText = sn;
            var un = editDialog.fields.unit || "";
            try { un = un || settingsCtl.settings["weight_unit_default"] || "unidad"; } catch (e) {}
            var ui = fUnit.model.indexOf(un);
            fUnit.currentIndex = ui >= 0 ? ui : 0;
            // Fase 3: flags desde attrs_json.
            var at = {};
            try { at = JSON.parse(editDialog.fields.attrsJson || "{}"); } catch (e) {}
            fTrackSerial.checked = !!at.track_serial;
            fReceta.checked = !!at.requires_prescription;
            fControlled.checked = !!at.controlled;
            fWarranty.text = at.warranty_months !== undefined ? String(at.warranty_months) : "";
        }
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
                    placeholderText: qsTr("Stock (admite decimales)")
                    validator: DoubleValidator { bottom: 0; decimals: 3 }
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
                ComboBox {
                    id: fUnit
                    model: ["unidad", "g", "kg", "ml", "l", "caja", "paquete", "metro"]
                    ToolTip.text: qsTr("Unidad de medida")
                    ToolTip.visible: hovered
                }
            }
            Label {
                // Fase 2: precio por kilo informativo para granel.
                visible: fUnit.currentText === "g" || fUnit.currentText === "kg"
                         || fUnit.currentText === "ml" || fUnit.currentText === "l"
                text: {
                    var p = parseFloat(fPrice.text) || 0;
                    var u = fUnit.currentText;
                    var perKg = (u === "g") ? p * 1000 : (u === "ml" ? p * 1000 : p);
                    return qsTr("Equivale a %1 por %2").arg(money(perKg)).arg(u === "ml" || u === "l" ? "L" : "kg");
                }
                font.pixelSize: Theme.fontS
                opacity: 0.7
            }
            RowLayout {
                ComboBox {
                    id: fCat
                    Layout.fillWidth: true
                    editable: true
                    model: rootCatNames()
                    ToolTip.text: qsTr("Categoría (lista de Configuración + libres)")
                    ToolTip.visible: hovered
                    onCurrentTextChanged: {
                        var sid = catIdByName(currentText);
                        fSubcat.model = subcatNames(sid);
                    }
                }
                ComboBox {
                    id: fSubcat
                    Layout.fillWidth: true
                    editable: true
                    model: []
                    ToolTip.text: qsTr("Subcategoría")
                    ToolTip.visible: hovered
                }
            }
            // Fase 3: lote + vencimiento (obligatorios si require_expiry).
            RowLayout {
                TextField {
                    id: fLote
                    text: editDialog.fields.lote || ""
                    placeholderText: qsTr("Lote")
                    Layout.fillWidth: true
                }
                TextField {
                    id: fVenc
                    text: editDialog.fields.vencimiento || ""
                    placeholderText: qsTr("Vence AAAA-MM-DD")
                    inputMask: "9999-99-99;_"
                    Layout.fillWidth: true
                }
            }
            Label {
                visible: {
                    try { return settingsCtl.settings["require_expiry"] === "1"; } catch (e) { return false; }
                }
                text: qsTr("Este negocio exige lote y vencimiento.")
                font.pixelSize: Theme.fontS
                color: Theme.warning
            }
            // Fase 3: flags por vertical (seriales, receta, controlado, garantía).
            GridLayout {
                columns: 2
                CheckBox {
                    id: fTrackSerial
                    text: qsTr("Lleva serial/IMEI")
                }
                CheckBox {
                    id: fReceta
                    text: qsTr("Requiere receta")
                }
                CheckBox {
                    id: fControlled
                    text: qsTr("Controlado (supervisor)")
                }
                RowLayout {
                    Label { text: qsTr("Garantía (meses)") }
                    TextField {
                        id: fWarranty
                        placeholderText: "12"
                        maximumLength: 3
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.preferredWidth: 70
                    }
                }
            }
            ComboBox {
                id: fTax
                Layout.fillWidth: true
                editable: true
                model: taxNames()
                ToolTip.text: qsTr("Impuesto de la ficha (lista de Configuración)")
                ToolTip.visible: hovered
            }
            Label {
                id: legacyTaxHint
                visible: {
                    var cur = fTax.editText !== "" ? fTax.editText : fTax.currentText;
                    return cur !== "" && taxNames().indexOf(cur) < 0;
                }
                text: qsTr("Ese impuesto no está en la lista: se liquidará la tasa por defecto.")
                font.pixelSize: Theme.fontS
                color: Theme.warning
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                id: editErr
                color: Theme.error
            }
            Label {
                // Ayuda reactiva: primer problema del formulario (validación en vivo)
                text: fName.text.trim() === "" ? qsTr("Ingrese el nombre") :
                      !fPrice.acceptableInput ? qsTr("Precio inválido (≥ 0)") :
                      !fStock.acceptableInput ? qsTr("Stock inválido (≥ 0, admite decimales)") : ""
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
            var taxVal = fTax.editText !== "" ? fTax.editText : fTax.currentText;
            var catVal = fCat.editText !== "" ? fCat.editText : fCat.currentText;
            var subVal = fSubcat.editText !== "" ? fSubcat.editText : fSubcat.currentText;
            // Fase 3: attrs_json (preserva claves desconocidas de la ficha).
            var at = {};
            try { at = JSON.parse(editDialog.fields.attrsJson || "{}"); } catch (e) {}
            at.track_serial = fTrackSerial.checked;
            at.requires_prescription = fReceta.checked;
            at.controlled = fControlled.checked;
            var wm = parseInt(fWarranty.text);
            if (!isNaN(wm) && wm > 0)
                at.warranty_months = wm;
            else
                delete at.warranty_months;
            var venc = fVenc.text.replace(/_/g, "").trim();
            if (venc === "----" || venc === "--" || venc === "")
                venc = "";
            var fields = {
                "name": fName.text,
                "price": parseFloat(fPrice.text) || 0,
                "stock": parseFloat(fStock.text) || 0,
                "cat": catVal || "General",
                "subcat": subVal || "",
                "unit": fUnit.currentText || "unidad",
                "tax": taxVal || defaultTaxName(),
                "lote": fLote.text.trim(),
                "vencimiento": venc,
                "attrsJson": JSON.stringify(at)
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
