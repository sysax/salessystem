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
    property bool loading: false
    property string pendingOp: "search"

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
            onAccepted: root.requestView()
        }
        Button {
            text: qsTr("Buscar")
            implicitHeight: 48
            onClicked: root.requestView()
        }
        BusyIndicator {
            visible: root.loading
            running: root.loading
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
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
            placeholderText: qsTr("Filtrar en servidor (nombre, SKU, categoría)…")
            Layout.fillWidth: true
            implicitHeight: 40
            onTextChanged: root.requestView()
        }
        Label {
            text: qsTr("%1 de %2").arg(productList.count).arg(catalog.productModel.totalCount)
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
        id: productList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: catalog.productModel
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 48
            onClicked: {
                editDialog.sku = model.sku;
                editDialog.fields = catalog.productModel.get(index);
                editDialog.open();
            }
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: model.sku
                    Layout.preferredWidth: 120
                    elide: Text.ElideRight
                }
                Label {
                    text: model.name
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: money(model.price)
                    Layout.preferredWidth: 110
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: Utils.formatQty(model.stock) + (model.unit ? " " + model.unit : "")
                          + ((model.reserved || 0) > 0 ? " · disp " + Utils.formatQty(model.available) : "")
                    Layout.preferredWidth: 140
                    horizontalAlignment: Text.AlignRight
                    color: model.stock <= 0 ? "red" : palette.text
                    font.bold: model.stock <= 0
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
        // Fase 4: scroll infinito (anexa el siguiente lote al llegar abajo).
        onMovementEnded: {
            if (catalog.productModel.canFetchMore && !root.loading) {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
        }
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: productList.count === 0 && !root.loading
        icon: "📦"
        title: catalog.productModel.totalCount === 0 ? qsTr("Sin productos") : qsTr("Sin resultados")
        hint: catalog.productModel.totalCount === 0 ? qsTr("Crea el primero con “Nuevo”, importa desde Compras o inicializa el catálogo del rubro en Configuración.") : qsTr("Ajusta el filtro o la búsqueda.")
        actionText: catalog.productModel.totalCount === 0 ? qsTr("Nuevo producto") : ""
        onAction: {
            editDialog.sku = "";
            editDialog.fields = {};
            editDialog.open();
        }
    }
    // Fase 4: pie incremental (en lugar del Pager discreto).
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSmall
        Label {
            text: qsTr("Mostrando %1 de %2").arg(productList.count).arg(catalog.productModel.totalCount)
            opacity: 0.7
            Layout.fillWidth: true
        }
        BusyIndicator {
            visible: root.loading && productList.count > 0
            running: root.loading && productList.count > 0
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
        }
        Button {
            text: qsTr("Cargar más")
            visible: catalog.productModel.canFetchMore
            onClicked: {
                root.pendingOp = "more";
                root.loading = true;
                queryTimer.restart();
            }
        }
    }

    // Multitienda: el rubro activo decide qué campos verticales se muestran.
    // Abarrotes no ve garantía/RMA/receta; farmacia no ve seriales, etc.
    function businessType() {
        try { return settingsCtl.settings["business_type"] || "miscelanea"; } catch (e) { return "miscelanea"; }
    }
    function supportsSerial() {
        var bt = businessType();
        return bt === "miscelanea" || bt === "celulares" || bt === "taller" || bt === "ferreteria";
    }
    function supportsExpiry() {
        var bt = businessType();
        if (bt === "miscelanea")
            return true;
        try { return settingsCtl.settings["require_expiry"] === "1"; } catch (e) {}
        return bt === "farmacia" || bt === "veterinaria" || bt === "abarrotes"
            || bt === "panaderia" || bt === "restaurante" || bt === "cafeteria";
    }
    function supportsReceta() {
        var bt = businessType();
        return bt === "miscelanea" || bt === "farmacia" || bt === "veterinaria" || bt === "consultorio";
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
        root.requestView();
    }

    // Fase 4: scroll infinito (texto + orden al modelo; lotes de 30).
    function queryText() {
        return filterField.text !== "" ? filterField.text : searchField.text;
    }
    function requestView() {
        root.pendingOp = "search";
        root.loading = true;
        queryTimer.restart();
    }
    function doView() {
        if (root.pendingOp === "more") {
            catalog.fetchMoreProducts();
        } else {
            catalog.searchProducts(root.queryText(), root.sortKey, root.sortAsc);
        }
        root.loading = false;
    }

    Timer {
        id: queryTimer
        interval: 5
        repeat: false
        onTriggered: root.doView()
    }

    Component.onCompleted: { root.requestView(); reloadCats(); }
    onVisibleChanged: if (visible) root.requestView()

    Connections {
        target: settingsCtl
        function onSettingsChanged() { root.reloadCats(); root.requestView(); }
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
                // Fase 2: importes >= 0 con máx. 2 decimales (MoneyField + Money backend).
                MoneyField {
                    id: fPrice
                    text: editDialog.fields.price !== undefined ? editDialog.fields.price : ""
                    placeholderText: qsTr("Precio")
                    label: qsTr("Precio")
                    maxValue: 999999999
                    allowNegative: false
                    maxDecimals: 2
                    Layout.fillWidth: true
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
            RowLayout {
                MoneyField {
                    id: fPriceBuy
                    text: editDialog.fields.priceBuy !== undefined ? editDialog.fields.priceBuy : ""
                    placeholderText: qsTr("P. compra")
                    label: qsTr("P. compra")
                    ToolTip.text: qsTr("Precio de compra (≥ 0, 2 decimales)")
                    ToolTip.visible: hovered
                    maxValue: 999999999
                    allowNegative: false
                    maxDecimals: 2
                    Layout.fillWidth: true
                }
                MoneyField {
                    id: fPriceWholesale
                    text: editDialog.fields.priceWholesale !== undefined ? editDialog.fields.priceWholesale : ""
                    placeholderText: qsTr("P. mayoreo")
                    label: qsTr("P. mayoreo")
                    ToolTip.text: qsTr("Precio mayoreo (≥ 0, 2 decimales)")
                    ToolTip.visible: hovered
                    maxValue: 999999999
                    allowNegative: false
                    maxDecimals: 2
                    Layout.fillWidth: true
                }
            }
            Label {
                // Fase 2: precio por kilo informativo para granel.
                visible: fUnit.currentText === "g" || fUnit.currentText === "kg"
                         || fUnit.currentText === "ml" || fUnit.currentText === "l"
                text: {
                    var p = 0;
                    try { p = fPrice.amount(); } catch (e) { p = parseFloat(fPrice.text) || 0; }
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
            // Multitienda: solo visible en rubros que manejan vencimiento.
            RowLayout {
                visible: supportsExpiry()
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
            // Multitienda: cada rubro solo ve los suyos (abarrotes no ve RMA...).
            GridLayout {
                columns: 2
                visible: supportsSerial() || supportsReceta()
                CheckBox {
                    id: fTrackSerial
                    text: qsTr("Lleva serial/IMEI")
                    visible: supportsSerial()
                }
                CheckBox {
                    id: fReceta
                    text: qsTr("Requiere receta")
                    visible: supportsReceta()
                }
                CheckBox {
                    id: fControlled
                    text: qsTr("Controlado (supervisor)")
                    visible: supportsReceta()
                }
                RowLayout {
                    visible: supportsSerial()
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
                      !fPrice.acceptableInput ? (fPrice.errorText || qsTr("Precio inválido (≥ 0, 2 decimales)")) :
                      !fPriceBuy.acceptableInput ? (fPriceBuy.errorText || qsTr("P. compra inválido (≥ 0)")) :
                      !fPriceWholesale.acceptableInput ? (fPriceWholesale.errorText || qsTr("P. mayoreo inválido (≥ 0)")) :
                      !fStock.acceptableInput ? qsTr("Stock inválido (≥ 0, admite decimales)") : ""
                color: Theme.error
                visible: text !== ""
            }
        }
        Component.onCompleted: {
            // Guardar solo con formulario válido (el chequeo backend en onAccepted se conserva)
            editDialog.standardButton(Dialog.Save).enabled = Qt.binding(function() {
                return fName.text.trim() !== "" && fPrice.acceptableInput && fPriceBuy.acceptableInput
                    && fPriceWholesale.acceptableInput && fStock.acceptableInput;
            });
        }
        onAccepted: {
            if (!fPrice.isValid() || !fPriceBuy.isValid() || !fPriceWholesale.isValid()) {
                editErr.text = fPrice.errorText || fPriceBuy.errorText || fPriceWholesale.errorText
                    || qsTr("Revise los importes (≥ 0, 2 decimales).");
                open();
                return;
            }
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
                "price": fPrice.amount(),
                "stock": parseFloat(fStock.text) || 0,
                "cat": catVal || "General",
                "subcat": subVal || "",
                "unit": fUnit.currentText || "unidad",
                "tax": taxVal || defaultTaxName(),
                "lote": fLote.text.trim(),
                "vencimiento": venc,
                "attrsJson": JSON.stringify(at)
            };
            // Solo enviar costos opcionales si se diligenciaron (≥ 0, 2 decimales).
            if (fPriceBuy.text.trim() !== "")
                fields["priceBuy"] = fPriceBuy.amount();
            if (fPriceWholesale.text.trim() !== "")
                fields["priceWholesale"] = fPriceWholesale.amount();
            var r;
            if (editDialog.sku === "") {
                fields.sku = "P" + Date.now().toString().slice(-6);
                r = catalog.add(fields);
            } else {
                r = catalog.update(editDialog.sku, fields, auth.currentUser);
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
