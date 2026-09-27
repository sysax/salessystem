// Fase 2: campo monetario reutilizable (par UI de Money::tryParse).
// Valida en entrada: máx. decimales, sin negativos donde no aplique y tope.
// Uso: MoneyField { id: cashField; label: qsTr("Efectivo"); maxDecimals: 2 }
// El padre muestra el error con: Label { text: cashField.errorText; ... }
// Locale es_CO: acepta "10.000,50", "10000.50" y "$ 10.000" (como el backend).
import QtQuick
import QtQuick.Controls
import QtSalesSystem

TextField {
    id: root

    property double maxValue: 999999999
    property bool allowNegative: false
    property int maxDecimals: 2
    property string label: ""
    property string errorText: ""

    placeholderText: label

    inputMethodHints: Qt.ImhFormattedNumbersOnly

    validator: DoubleValidator {
        bottom: root.allowNegative ? -root.maxValue : 0
        top: root.maxValue
        decimals: root.maxDecimals
        notation: DoubleValidator.StandardNotation
        locale: "es_CO"
    }

    // Fondo propio mínimo para el borde rojo de inválido (Material no expone borde).
    background: Rectangle {
        color: "transparent"
        border.color: root.errorText !== "" ? Theme.error : (root.activeFocus ? Theme.accent : "#BDBDBD")
        border.width: (root.errorText !== "" || root.activeFocus) ? 2 : 1
        radius: 4
    }

    onLabelChanged: {
        if (placeholderText === "" && label !== "")
            placeholderText = label;
    }

    // Normaliza como Money::tryParse: quita $, COP y espacios; '.' miles y
    // ',' decimal en es_CO ("1.850.000,50" => "1850000.50").
    function normalized() {
        var t = (text || "").trim();
        t = t.split("$").join("").split(" ").join("");
        t = t.replace(/COP/gi, "");
        if (t === "")
            return "";
        var dots = (t.match(/\./g) || []).length;
        var commas = (t.match(/,/g) || []).length;
        var norm = t;
        if (dots > 0 && commas > 0) {
            norm = norm.split(".").join("").replace(",", ".");
            // Si quedara más de una coma (malformado), colapsa a la primera.
            var parts = norm.split(".");
            if (parts.length > 2)
                norm = parts.shift() + "." + parts.join("");
        } else if (commas > 0) {
            norm = norm.replace(/,/g, ".");
            var p2 = norm.split(".");
            if (p2.length > 2)
                norm = p2.shift() + "." + p2.join("");
        } else if (dots > 1) {
            norm = norm.split(".").join("");
        } else if (dots === 1) {
            var pos = norm.lastIndexOf(".");
            var dec = norm.length - pos - 1;
            if (dec < 1 || dec > root.maxDecimals)
                norm = norm.split(".").join("");
        }
        return norm;
    }

    function parsedValue() {
        var n = normalized();
        if (n === "" || n === "-" || n === "+" || n === ".")
            return NaN;
        return parseFloat(n);
    }

    // Valida regex de decimales + rango; publica errorText y borde rojo.
    function isValid() {
        var raw = (text || "").trim();
        if (raw === "") {
            // Vacío: no ensuciar con error permanente (el padre decide si exige dato).
            errorText = "";
            return acceptableInput;
        }
        var n = normalized();
        var decRe = new RegExp("^[+-]?\\d+(\\.\\d{1," + root.maxDecimals + "})?$");
        if (!decRe.test(n)) {
            errorText = qsTr("Máximo %1 decimales").arg(root.maxDecimals);
            return false;
        }
        var v = parseFloat(n);
        if (isNaN(v) || !isFinite(v)) {
            errorText = qsTr("Importe inválido");
            return false;
        }
        if (!root.allowNegative && v < 0) {
            errorText = qsTr("No admite negativos");
            return false;
        }
        if (Math.abs(v) > root.maxValue + 1e-9) {
            errorText = qsTr("Máximo %1").arg(root.maxValue);
            return false;
        }
        // Detección de decimales ocultos por redondeo ("10,999" con maxDecimals 2).
        var c = Math.round(v * 100) / 100;
        if (root.maxDecimals <= 2 && Math.abs(v * 100 - Math.round(v * 100)) > 0.001) {
            errorText = qsTr("Máximo %1 decimales").arg(root.maxDecimals);
            return false;
        }
        void c;
        errorText = "";
        return acceptableInput;
    }

    // Importe redondeado al céntimo (par de Money::fromCop).
    function cents() {
        var v = parsedValue();
        if (isNaN(v) || !isFinite(v))
            return 0;
        return Math.round(v * 100);
    }

    function amount() {
        return cents() / 100;
    }

    onTextChanged: isValid()
}
