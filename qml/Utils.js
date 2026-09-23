.pragma library

// Registro explícito desde Main.qml (Component.onCompleted).
// Evita el lookup frágil vía Qt.application.windows, que puede ser
// undefined/vacío según plataforma o momento de la llamada.
var _toast = null;
var _loading = null;

function registerToast(t) { _toast = t; }
function registerLoading(l) { _loading = l; }

// Función segura para mostrar Toasts
function showToast(type, message, duration) {
    if (_toast) {
        if (type === "success") _toast.success(message, duration);
        else if (type === "error") _toast.error(message, duration);
        else if (type === "warning") _toast.warning(message, duration);
        else if (type === "info") _toast.info(message, duration);
        else _toast.show(message, duration, type);
        return;
    }
    console.log("Toast (" + type + "): " + message);
}

function showLoading(message) {
    if (_loading) {
        _loading.show(message);
        return;
    }
    console.log("Loading: " + message);
}

function hideLoading() {
    if (_loading) {
        _loading.hide();
        return;
    }
}

// Fase 2: cantidad con hasta 3 decimales recortando ceros (2, 0.35).
// Única función de formato de cantidades en QML (par de TicketPrinter::formatQty).
function formatQty(v) {
    var n = Number(v) || 0;
    var s = n.toFixed(3);
    s = s.replace(/\.?0+$/, "");
    return s === "-0" ? "0" : s;
}

function isWeighable(unit) {
    var u = String(unit || "").toLowerCase();
    return u === "g" || u === "kg" || u === "ml" || u === "l";
}
