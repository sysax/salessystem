// Configuración del negocio (Fase 0 multinegocio): Negocio / Moneda e
// Impuestos / Vertical. Solo rol Administrador (ver canEdit + sidebar).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ScrollView {
    id: root
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacingSmall

        Label {
            text: qsTr("Configuración del negocio")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        Label {
            id: msg
            visible: text !== ""
            color: Theme.error
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            id: okMsg
            visible: text !== ""
            color: Theme.success
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        GroupBox {
            title: qsTr("Negocio")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingSmall
                TextField {
                    id: fName
                    placeholderText: qsTr("Nombre del negocio")
                    Layout.fillWidth: true
                }
                RowLayout {
                    TextField {
                        id: fNit
                        placeholderText: qsTr("NIT")
                        Layout.fillWidth: true
                    }
                    ComboBox {
                        id: fType
                        Layout.fillWidth: true
                        model: ["miscelanea", "farmacia", "abarrotes", "celulares", "ferreteria", "ropa", "restaurante", "cafeteria", "panaderia", "peluqueria", "taller", "lavanderia", "consultorio", "veterinaria"]
                    }
                }
                TextField {
                    id: fAddress
                    placeholderText: qsTr("Dirección (ticket)")
                    Layout.fillWidth: true
                }
                TextField {
                    id: fPhone
                    placeholderText: qsTr("Teléfono (ticket)")
                    Layout.fillWidth: true
                }
            }
        }

        GroupBox {
            title: qsTr("Moneda e impuestos (Colombia)")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingSmall
                RowLayout {
                    TextField {
                        id: fSymbol
                        placeholderText: qsTr("Símbolo ($)")
                        maximumLength: 4
                        Layout.preferredWidth: 120
                    }
                    TextField {
                        id: fDecimals
                        placeholderText: qsTr("Decimales (0-2)")
                        maximumLength: 1
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.preferredWidth: 140
                    }
                    TextField {
                        id: fTax
                        placeholderText: qsTr("IVA por defecto %")
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                }
                TextField {
                    id: fMora
                    placeholderText: qsTr("Mora mensual % (CxC)")
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Tasas de impuesto (nombre y %)")
                    font.bold: true
                }
                // Fase 1: lista editable ligada a tax_rates_json.
                ColumnLayout {
                    id: ratesBox
                    Layout.fillWidth: true
                    spacing: 4
                    property var rates: []
                    Repeater {
                        model: ratesBox.rates
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: modelData.name + " — " + modelData.rate + "%"
                                Layout.fillWidth: true
                            }
                            Button {
                                text: qsTr("Quitar")
                                enabled: ratesBox.rates.length > 1
                                         && String(modelData.rate) !== fTax.text.trim()
                                onClicked: {
                                    var r = ratesBox.rates.slice();
                                    r.splice(index, 1);
                                    ratesBox.rates = r;
                                    root.refreshRates();
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    TextField {
                        id: newRateName
                        placeholderText: qsTr("Nombre (ej. IVA 5%)")
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: newRateValue
                        placeholderText: qsTr("%")
                        maximumLength: 6
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.preferredWidth: 90
                    }
                    Button {
                        text: qsTr("Añadir")
                        onClicked: {
                            var v = parseFloat(newRateValue.text);
                            if (newRateName.text.trim() === "" || isNaN(v) || v < 0 || v > 100) {
                                msg.text = qsTr("Tasa inválida: nombre y % entre 0 y 100.");
                                return;
                            }
                            msg.text = "";
                            var r = ratesBox.rates.slice();
                            r.push({"name": newRateName.text.trim(), "rate": v});
                            ratesBox.rates = r;
                            root.refreshRates();
                            newRateName.text = "";
                            newRateValue.text = "";
                        }
                    }
                }
                Label {
                    text: qsTr("La tasa por defecto debe estar en la lista y no puede quitarse.")
                    font.pixelSize: Theme.fontS
                    opacity: 0.7
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        GroupBox {
            title: qsTr("Vertical (fases siguientes)")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                CheckBox {
                    id: fExpiry
                    text: qsTr("Exigir lote + vencimiento (farmacia)")
                }
                CheckBox {
                    id: fSerial
                    text: qsTr("Exigir serial/IMEI (celulares)")
                }
                Label {
                    text: qsTr("Estos flags se aplican desde la Fase 4; aquí solo se guardan.")
                    font.pixelSize: Theme.fontS
                    opacity: 0.7
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        GroupBox {
            title: qsTr("Catálogo inicial por rubro (Fase 2)")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingSmall
                Label {
                    text: qsTr("Solo añade categorías y productos de ejemplo del rubro elegido. No borra nada existente; puede aplicarse varias veces sin duplicar.")
                    font.pixelSize: Theme.fontS
                    opacity: 0.7
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    Button {
                        text: qsTr("Inicializar catálogo")
                        enabled: settingsCtl.canEdit()
                        onClicked: seedDialog.open()
                    }
                    Label {
                        id: seedMsg
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        RowLayout {
            Button {
                text: qsTr("Recargar")
                onClicked: root.loadAll()
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Guardar")
                enabled: settingsCtl.canEdit()
                highlighted: true
                onClicked: {
                    msg.text = "";
                    okMsg.text = "";
                    // Fase 1: la tasa por defecto debe estar en la lista.
                    var dflt = fTax.text.trim();
                    var found = false;
                    for (var i = 0; i < ratesBox.rates.length; ++i) {
                        if (String(ratesBox.rates[i].rate) === dflt
                            || parseFloat(ratesBox.rates[i].rate) === parseFloat(dflt))
                            found = true;
                    }
                    if (!found) {
                        msg.text = qsTr("La tasa por defecto (%1%) no está en la lista: añádela primero.").arg(dflt);
                        return;
                    }
                    var r = settingsCtl.save({
                        "business_name": fName.text,
                        "business_nit": fNit.text,
                        "business_type": fType.currentText,
                        "business_address": fAddress.text,
                        "business_phone": fPhone.text,
                        "currency_symbol": fSymbol.text,
                        "currency_decimals": fDecimals.text,
                        "default_tax_rate": fTax.text,
                        "tax_rates_json": JSON.stringify(ratesBox.rates),
                        "mora_rate_monthly": fMora.text,
                        "require_expiry": fExpiry.checked ? "1" : "0",
                        "require_serial": fSerial.checked ? "1" : "0"
                    });
                    if (r.ok) {
                        okMsg.text = qsTr("Guardado. Los próximos tickets y reportes usan estos datos.");
                    } else {
                        msg.text = r.error;
                    }
                }
            }
        }
    }

    function loadAll() {
        settingsCtl.load();
        root.fillFields();
    }

    function refreshRates() {
        // Reasignar para que el Repeater se actualice.
        var r = ratesBox.rates;
        ratesBox.rates = [];
        ratesBox.rates = r;
    }

    function fillFields() {
        var s = settingsCtl.settings;
        fName.text = s["business_name"] || "";
        fNit.text = s["business_nit"] || "";
        fAddress.text = s["business_address"] || "";
        fPhone.text = s["business_phone"] || "";
        fSymbol.text = s["currency_symbol"] || "$";
        fDecimals.text = s["currency_decimals"] || "0";
        fTax.text = s["default_tax_rate"] || "19";
        fMora.text = s["mora_rate_monthly"] || "2";
        try {
            var arr = JSON.parse(s["tax_rates_json"] || "[]");
            ratesBox.rates = arr.length > 0 ? arr : [{ "name": "IVA 19%", "rate": 19 },
                                                     { "name": "Excluido", "rate": 0 }];
        } catch (e) {
            ratesBox.rates = [{ "name": "IVA 19%", "rate": 19 },
                              { "name": "Excluido", "rate": 0 }];
        }
        root.refreshRates();
        var bt = s["business_type"] || "miscelanea";
        var idx = fType.model.indexOf(bt);
        fType.currentIndex = idx >= 0 ? idx : 0;
        fExpiry.checked = (s["require_expiry"] || "0") === "1";
        fSerial.checked = (s["require_serial"] || "0") === "1";
    }

    Component.onCompleted: loadAll()

    Dialog {
        id: seedDialog
        title: qsTr("Inicializar catálogo")
        modal: true
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: 280
            Label {
                text: qsTr("¿Añadir categorías y productos de ejemplo para \"%1\"? No se borra nada.").arg(fType.currentText)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            seedMsg.text = "";
            if (!db.applySeedFile(fType.currentText)) {
                seedMsg.text = qsTr("No hay seed para ese rubro: ") + db.statusMessage;
                return;
            }
            settingsCtl.load();
            catalog.reloadCategories(fType.currentText);
            catalog.search("");
            okMsg.text = qsTr("Catálogo de \"%1\" añadido.").arg(fType.currentText);
        }
    }

    Connections {
        target: settingsCtl
        // Solo rellenar: load() re-emite settingsChanged y recursaría.
        function onSettingsChanged() {
            root.fillFields();
        }
    }
}
