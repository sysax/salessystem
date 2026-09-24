// Punto de venta: carrito, pagos mixtos, promos, ticket, caja (antes pos.py).
// Mejora #4 POS mejorado: botones táctiles 48px, confirmación al eliminar, swipe gestures.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem
import "components"

RowLayout {
    id: root
    spacing: Theme.spacingMedium

    readonly property int touchH: 48   // objetivo táctil mínimo
    readonly property int rowHProduct: 56
    readonly property int rowHCart: 64

    function addProduct(productId) {
        // Fase 2: si es pesable, pedir cantidad decimal; si no, agregar 1.
        var p = null;
        var prods = catalog.products || [];
        for (var i = 0; i < prods.length; ++i) {
            if (prods[i].id === productId) {
                p = prods[i];
                break;
            }
        }
        if (p && Utils.isWeighable(p.unit)) {
            qtyDialog.productId = productId;
            qtyDialog.productName = p.name + " (" + money(p.price) + " / " + (p.unit || "kg") + ")";
            qtyField.text = "1";
            qtyErr.text = "";
            qtyDialog.open();
            return;
        }
        var r = pos.addToCart(productId, 1);
        if (!r.ok) {
            Utils.showToast("error", r.error, 3000);
            return;
        }
        // Fase 3: producto con serial → pedir IMEI para la línea recién creada.
        if (r.needsSerial) {
            serialDialog.cartIndex = pos.cart.length - 1;
            serialDialog.productId = productId;
            serialField.text = "";
            serialErr.text = "";
            serialDialog.open();
        } else {
            Utils.showToast("success", "Agregado al carrito", 1500);
        }
        // Fase 3: producto con receta → pedir Nº (no bloquea, valida al cobrar).
        if (p && root.needsReceta(p)) {
            recetaDialog.cartIndex = pos.cart.length - 1;
            recetaDialog.productName = p.name;
            recetaField.text = "";
            recetaErr.text = "";
            recetaDialog.open();
        }
    }

    function needsReceta(p) {
        try {
            var at = JSON.parse(p.attrsJson || "{}");
            return !!at.requires_prescription;
        } catch (e) {
            return false;
        }
    }

    // Fase 3: ¿alguna línea del carrito exige receta sin informar?
    function missingReceta() {
        var cart = pos.cart || [];
        var prods = catalog.products || [];
        for (var i = 0; i < cart.length; ++i) {
            var line = cart[i];
            if (line.receta && line.receta !== "")
                continue;
            for (var j = 0; j < prods.length; ++j) {
                if (prods[j].id === line.productId && root.needsReceta(prods[j]))
                    return prods[j].name;
            }
        }
        return "";
    }

    // Fase 2: visibleProducts = filtro local por categoría.
    property var visibleProducts: []

    function refreshProducts() {
        var out = [];
        var prods = catalog.products || [];
        for (var i = 0; i < prods.length; ++i) {
            if (root.productVisible(prods[i]))
                out.push(prods[i]);
        }
        root.visibleProducts = out;
    }

    Component.onCompleted: root.refreshProducts()

    // Fase 4 (abarrotes): foco listo para escáner hardware al entrar al POS.
    function focusScanner() {
        searchField.forceActiveFocus();
    }

    Connections {
        target: catalog
        function onProductsChanged() { root.refreshProducts(); }
        function onCategoriesChanged() {
            catFilter.model = root.catNames();
            root.refreshProducts();
        }
    }

    function catNames() {
        var out = ["Todas"];
        var cats = [];
        try { cats = catalog.categories || []; } catch (e) {}
        for (var i = 0; i < cats.length; ++i) {
            if (((cats[i].parentId || 0) === 0) && out.indexOf(cats[i].name) < 0)
                out.push(cats[i].name);
        }
        return out;
    }

    function productVisible(m) {
        if (!m)
            return false;
        var f = catFilter.currentText || "Todas";
        if (!f || f === "Todas" || f === qsTr("Todas"))
            return true;
        if ((m.cat || "") === f)
            return true;
        // Subcategoría cuya madre es la seleccionada
        var cats = [];
        try { cats = catalog.categories || []; } catch (e) {}
        for (var k = 0; k < cats.length; ++k) {
            if (cats[k].name === (m.subcat || "") && cats[k].parentId) {
                for (var l = 0; l < cats.length; ++l) {
                    if (cats[l].id === cats[k].parentId && cats[l].name === f)
                        return true;
                }
            }
        }
        return false;
    }

    function askRemoveLine(idx, name) {
        confirmRemoveDialog.removeIndex = idx;
        confirmRemoveDialog.removeName = name || "";
        confirmRemoveDialog.open();
    }

    // Lógica de cobro: Efectivo/Mixto exigen efectivo >= total (Crédito y demás, no).
    function cashNeeded() {
        return methodBox.currentText === "Efectivo" || methodBox.currentText === "Mixto";
    }
    function cashValue() {
        return parseFloat(cashField.text) || 0;
    }
    function cartTotal() {
        return (pos.totals && pos.totals.total) || 0;
    }
    function canCharge() {
        if (pos.cart.length === 0)
            return false;
        if (!cashNeeded())
            return true;
        if (cartTotal() <= 0)
            return true; // promo cubre el total
        return cashField.acceptableInput && cashValue() >= cartTotal();
    }

    // Izquierda: búsqueda + carrito
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.spacingSmall

        RowLayout {
            spacing: Theme.spacingSmall
            TextField {
                id: searchField
                placeholderText: qsTr("Buscar producto (nombre, SKU, código)")
                Layout.fillWidth: true
                implicitHeight: root.touchH
                focus: true
                onAccepted: catalog.search(text)
                onVisibleChanged: if (visible) forceActiveFocus()
            }
            Button {
                text: qsTr("Buscar")
                implicitHeight: root.touchH
                implicitWidth: 110
                onClicked: catalog.search(searchField.text)
            }
        }
        RowLayout {
            spacing: Theme.spacingSmall
            // Fase 2: filtro por categoría (local, sobre catalog.products).
            ComboBox {
                id: catFilter
                Layout.fillWidth: true
                implicitHeight: root.touchH
                model: root.catNames()
                onCurrentTextChanged: {
                    root.refreshProducts();
                    productList.positionViewAtBeginning();
                }
            }
        }
        ListView {
            id: productList
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            clip: true
            model: root.visibleProducts
            visible: root.visibleProducts.length > 0
            delegate: SwipeDelegate {
                id: prodDelegate
                width: ListView.view.width
                height: root.rowHProduct
                text: modelData.name + "  ·  " + money(modelData.price)
                      + (modelData.unit && modelData.unit !== "unidad" ? " / " + modelData.unit : "")
                      + "  ·  stock " + Utils.formatQty(modelData.stock)
                font.pixelSize: Theme.fontM
                onClicked: root.addProduct(modelData.id)
                // Deslizar para revelar acción táctil "Añadir"
                swipe.right: Button {
                    text: qsTr("Añadir +")
                    implicitHeight: root.rowHProduct
                    implicitWidth: 120
                    onClicked: {
                        prodDelegate.swipe.close();
                        root.addProduct(modelData.id);
                    }
                }
                swipe.onCompleted: {
                    // Swipe completo a la derecha también agrega
                    if (swipe.position > 0.5) {
                        swipe.close();
                        root.addProduct(modelData.id);
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        EmptyState {
            Layout.fillWidth: true
            visible: root.visibleProducts.length === 0
            icon: "🔍"
            title: qsTr("Sin productos para vender")
            hint: qsTr("Busca de nuevo, cambia la categoría o da de alta productos en el catálogo.")
        }
        RowLayout {
            spacing: Theme.spacingSmall
            Label {
                text: qsTr("Carrito (%1)").arg(pos.cart.length)
                font.bold: true
                font.pixelSize: Theme.fontML
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Vaciar")
                implicitHeight: root.touchH
                visible: pos.cart.length > 0
                onClicked: confirmClearDialog.open()
            }
        }
        Label {
            visible: pos.cart.length === 0
            text: qsTr("Carrito vacío — toca un producto para agregar. Desliza un producto para añadir rápido.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            opacity: 0.7
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pos.cart
            delegate: SwipeDelegate {
                id: cartDelegate
                width: ListView.view.width
                height: root.rowHCart
                // Deslizar a la izquierda revela Eliminar
                swipe.left: Button {
                    text: qsTr("Eliminar")
                    implicitHeight: root.rowHCart
                    implicitWidth: 120
                    onClicked: {
                        cartDelegate.swipe.close();
                        root.askRemoveLine(index, modelData.name);
                    }
                }
                swipe.onCompleted: {
                    if (swipe.position < -0.5) {
                        swipe.close();
                        root.askRemoveLine(index, modelData.name);
                    }
                }
                contentItem: RowLayout {
                    spacing: Theme.spacingSmall
                    Label {
                        text: modelData.name + " × " + Utils.formatQty(modelData.qty)
                              + (modelData.unit && modelData.unit !== "unidad" ? " " + modelData.unit : "")
                              + (modelData.serial ? "\nSN: " + modelData.serial : "")
                              + (modelData.receta ? " · Rx: " + modelData.receta : "")
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontM
                    }
                    Button {
                        text: "−"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontL
                        Accessible.name: qsTr("Disminuir cantidad")
                        // Fase 2: en pesables se edita exacto; en enteros, pasos de 1.
                        onClicked: {
                            if (Utils.isWeighable(modelData.unit))
                                root.editCartQty(index, modelData);
                            else
                                pos.setQty(index, modelData.qty - 1);
                        }
                        enabled: modelData.qty > 1 || Utils.isWeighable(modelData.unit)
                    }
                    Button {
                        text: "+"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontL
                        Accessible.name: qsTr("Aumentar cantidad")
                        onClicked: {
                            if (Utils.isWeighable(modelData.unit))
                                root.editCartQty(index, modelData);
                            else
                                pos.setQty(index, modelData.qty + 1);
                        }
                    }
                    Label {
                        text: money(modelData.subtotal)
                        Layout.preferredWidth: 110
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontM
                    }
                    Button {
                        text: "✕"
                        implicitWidth: root.touchH
                        implicitHeight: root.touchH
                        font.pixelSize: Theme.fontML
                        Accessible.name: qsTr("Eliminar del carrito")
                        onClicked: root.askRemoveLine(index, modelData.name)
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }

    // Derecha: totales + cobro + caja
    ColumnLayout {
        Layout.preferredWidth: 320
        Layout.fillHeight: true
        spacing: Theme.spacingSmall

        GroupBox {
            title: qsTr("Totales")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 4
                Label {
                    text: qsTr("Subtotal: ") + money(pos.totals.subtotal)
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("Descuento: ") + money(pos.totals.discount)
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("Impuestos: ") + money(pos.totals.tax)
                    font.pixelSize: Theme.fontM
                }
                // Fase 1: desglose por tasa (solo si hay más de una)
                Repeater {
                    model: (pos.totals.taxBreakdown && pos.totals.taxBreakdown.length > 1)
                           ? pos.totals.taxBreakdown : []
                    Label {
                        text: "  · " + modelData.name + ": " + money(modelData.tax)
                        font.pixelSize: Theme.fontS
                        opacity: 0.8
                    }
                }
                Label {
                    text: qsTr("TOTAL: ") + money(pos.totals.total)
                    font.bold: true
                    font.pixelSize: Theme.fontXL
                }
            }
        }
        RowLayout {
            spacing: Theme.spacingSmall
            TextField {
                id: promoField
                placeholderText: qsTr("Código promo")
                Layout.fillWidth: true
                implicitHeight: root.touchH
            }
            Button {
                text: qsTr("Aplicar")
                implicitHeight: root.touchH
                implicitWidth: 100
                onClicked: {
                    var r = pos.applyPromo(promoField.text);
                    if (r.ok)
                        Utils.showToast("success", "Descuento: " + money(r.discount), 2500);
                    else
                        Utils.showToast("error", r.error, 3000);
                }
            }
        }
        ComboBox {
            id: methodBox
            Layout.fillWidth: true
            implicitHeight: root.touchH
            model: ["Efectivo", "Transferencia", "Tarjeta", "Credito", "Mixto"]
        }
        TextField {
            id: cashField
            placeholderText: qsTr("Efectivo recibido")
            Layout.fillWidth: true
            implicitHeight: root.touchH
            inputMethodHints: Qt.ImhDigitsOnly
            validator: DoubleValidator { bottom: 0 }
        }
        TextField {
            id: clientField
            placeholderText: qsTr("Cliente (opcional)")
            Layout.fillWidth: true
            implicitHeight: root.touchH
        }
        Label {
            // Ayuda reactiva: cambio o faltante según el efectivo ingresado
            visible: root.cashNeeded() && pos.cart.length > 0 && cashField.text !== ""
            text: root.cashValue() >= root.cartTotal()
                  ? qsTr("Cambio: ") + money(root.cashValue() - root.cartTotal())
                  : qsTr("Faltan: ") + money(root.cartTotal() - root.cashValue())
            color: root.cashValue() >= root.cartTotal() ? Theme.success : Theme.error
            font.bold: true
            font.pixelSize: Theme.fontM
        }
        Button {
            text: qsTr("Cobrar")
            highlighted: true
            Layout.fillWidth: true
            implicitHeight: 56
            font.bold: true
            font.pixelSize: Theme.fontL
            enabled: root.canCharge()
            onClicked: {
                var pays = {};
                if (methodBox.currentText === "Mixto" || methodBox.currentText === "Efectivo") {
                    var cash = parseFloat(cashField.text) || 0;
                    if (cash > 0)
                        pays["efectivo"] = cash;
                }
                Utils.showLoading(qsTr("Procesando venta..."));
                var missing = root.missingReceta();
                if (missing !== "") {
                    Utils.hideLoading();
                    Utils.showToast("error", qsTr("Receta requerida para: ") + missing, 4000);
                    return;
                }
                var r = pos.checkout(clientField.text, pays, methodBox.currentText, auth.currentUser, auth.currentRole);
                Utils.hideLoading();
                if (r.ok) {
                    Utils.showToast("success", "Venta " + r.saleId + " · Cambio " + money(r.change), 3000);
                    cashField.text = "";
                    promoField.text = "";
                } else {
                    Utils.showToast("error", r.error, 4000);
                }
            }
        }
        GroupBox {
            title: qsTr("Caja")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: pos.caja.open ? qsTr("Abierta · esperado %1").arg(money(pos.caja.expected)) : qsTr("Cerrada")
                    font.pixelSize: Theme.fontM
                }
                RowLayout {
                    spacing: Theme.spacingSmall
                    TextField {
                        id: cajaField
                        placeholderText: qsTr("Monto")
                        Layout.fillWidth: true
                        implicitHeight: root.touchH
                    }
                    Button {
                        text: pos.caja.open ? qsTr("Cerrar") : qsTr("Abrir")
                        implicitHeight: root.touchH
                        implicitWidth: 100
                        onClicked: {
                            var r = pos.caja.open ? pos.closeCaja(parseFloat(cajaField.text) || 0, auth.currentUser) : pos.openCaja(parseFloat(cajaField.text) || 0, auth.currentUser);
                            if (r.ok) {
                                if (r.diff !== undefined)
                                    Utils.showToast("warning", "Diferencia: " + money(r.diff), 3000);
                                else
                                    Utils.showToast("success", "Caja abierta", 2500);
                            } else {
                                Utils.showToast("error", r.error, 4000);
                            }
                        }
                    }
                }
            }
        }
    }

    // Fase 2: editar cantidad exacta de una línea pesable del carrito.
    function editCartQty(idx, line) {
        qtyDialog.productId = -1;
        qtyDialog.cartIndex = idx;
        qtyDialog.productName = (line.name || "") + (line.unit ? " (" + line.unit + ")" : "");
        qtyField.text = Utils.formatQty(line.qty);
        qtyErr.text = "";
        qtyDialog.open();
    }

    // Confirmación antes de eliminar una línea (acción destructiva)
    Dialog {
        id: confirmRemoveDialog
        title: qsTr("Eliminar producto")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property int removeIndex: -1
        property string removeName: ""
        Label {
            text: qsTr("¿Eliminar \"%1\" del carrito?").arg(confirmRemoveDialog.removeName)
            wrapMode: Text.WordWrap
            width: 280
        }
        onAccepted: {
            if (removeIndex >= 0) {
                pos.removeLine(removeIndex);
                Utils.showToast("info", "Producto eliminado", 2000);
            }
            removeIndex = -1;
        }
        onRejected: removeIndex = -1
    }

    Dialog {
        id: confirmClearDialog
        title: qsTr("Vaciar carrito")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: qsTr("¿Vaciar todo el carrito? Esta acción no se puede deshacer.")
            wrapMode: Text.WordWrap
            width: 280
        }
        onAccepted: {
            pos.clearCart();
            Utils.showToast("info", "Carrito vaciado", 2000);
        }
    }

    // Fase 2: cantidad decimal para productos a granel (agregar o editar línea).
    Dialog {
        id: qtyDialog
        title: qsTr("Cantidad")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property int productId: -1
        property int cartIndex: -1
        property string productName: ""
        onOpened: qtyField.forceActiveFocus()
        ColumnLayout {
            width: 280
            Label {
                text: qtyDialog.productName
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.bold: true
            }
            TextField {
                id: qtyField
                placeholderText: qsTr("Cantidad (ej. 0.350)")
                Layout.fillWidth: true
                validator: DoubleValidator { bottom: 0.001; decimals: 3 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            Label {
                id: qtyErr
                color: Theme.error
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            var q = parseFloat(qtyField.text) || 0;
            if (q <= 0) {
                qtyErr.text = qsTr("Cantidad mayor a 0.");
                open();
                return;
            }
            if (qtyDialog.cartIndex >= 0) {
                pos.setQty(qtyDialog.cartIndex, q);
                Utils.showToast("success", "Cantidad: " + Utils.formatQty(q), 1500);
            } else {
                var r = pos.addToCart(qtyDialog.productId, q);
                if (!r.ok) {
                    qtyErr.text = r.error;
                    open();
                    return;
                }
                Utils.showToast("success", "Agregado: " + Utils.formatQty(q), 1500);
            }
            qtyDialog.cartIndex = -1;
            qtyDialog.productId = -1;
        }
        onRejected: {
            qtyDialog.cartIndex = -1;
            qtyDialog.productId = -1;
        }
    }

    // Fase 3: serial/IMEI para la línea recién agregada (y edición).
    Dialog {
        id: serialDialog
        title: qsTr("Serial / IMEI")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property int cartIndex: -1
        property int productId: -1
        onOpened: {
            serialField.forceActiveFocus();
            serialList.model = serialDialog.productId >= 0
                ? pos.inStockSerials(serialDialog.productId) : [];
        }
        ColumnLayout {
            width: 280
            Label {
                text: qsTr("Escanee o seleccione el serial:")
                Layout.fillWidth: true
            }
            TextField {
                id: serialField
                placeholderText: qsTr("Serial / IMEI")
                Layout.fillWidth: true
            }
            ListView {
                id: serialList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(160, (count || 0) * 40)
                visible: (count || 0) > 0
                clip: true
                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: modelData.serial + (modelData.imei2 ? " / " + modelData.imei2 : "")
                    onClicked: serialField.text = modelData.serial
                }
                ScrollBar.vertical: ScrollBar {}
            }
            Label {
                id: serialErr
                color: Theme.error
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            if (serialField.text.trim() === "") {
                serialErr.text = qsTr("Serial requerido para este producto.");
                open();
                return;
            }
            var r = pos.setLineSerial(serialDialog.cartIndex, serialField.text);
            if (!r.ok) {
                serialErr.text = r.error;
                open();
                return;
            }
            Utils.showToast("success", "Serial registrado", 1500);
        }
    }

    // Fase 3: Nº de receta para productos que la exigen.
    Dialog {
        id: recetaDialog
        title: qsTr("Receta médica")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        property int cartIndex: -1
        property string productName: ""
        onOpened: recetaField.forceActiveFocus()
        ColumnLayout {
            width: 280
            Label {
                text: qsTr("Nº de receta para \"%1\":").arg(recetaDialog.productName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextField {
                id: recetaField
                placeholderText: qsTr("Nº receta")
                Layout.fillWidth: true
            }
            Label {
                id: recetaErr
                color: Theme.error
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            if (recetaField.text.trim() === "") {
                recetaErr.text = qsTr("Indique el Nº de receta (se valida al cobrar).");
                open();
                return;
            }
            pos.setLineReceta(recetaDialog.cartIndex, recetaField.text);
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
