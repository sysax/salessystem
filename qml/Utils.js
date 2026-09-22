.pragma library

// Función segura para mostrar Toasts
// Intenta obtener la ventana activa y llama al globalToast
function showToast(type, message, duration) {
    // Pequeño delay para asegurar que la UI esté lista si se llama muy temprano
    if (typeof Qt !== 'undefined') {
        var component = Qt.createComponent("qrc:/qt/qml/QtSalesSystem/qml/Main.qml");
        // Fallback directo si tenemos acceso a la aplicación
        if (Qt.application.windows.length > 0) {
            var win = Qt.application.windows[0];
            if (win.globalToast) {
                if (type === "success") win.globalToast.success(message, duration);
                else if (type === "error") win.globalToast.error(message, duration);
                else if (type === "warning") win.globalToast.warning(message, duration);
                else if (type === "info") win.globalToast.info(message, duration);
                else win.globalToast.show(message, type, duration);
                return;
            }
        }
    }
    console.log("Toast (" + type + "): " + message);
}

function showLoading(message) {
    if (Qt.application.windows.length > 0) {
        var win = Qt.application.windows[0];
        if (win.globalLoading) {
            win.globalLoading.show(message);
            return;
        }
    }
    console.log("Loading: " + message);
}

function hideLoading() {
    if (Qt.application.windows.length > 0) {
        var win = Qt.application.windows[0];
        if (win.globalLoading) {
            win.globalLoading.hide();
            return;
        }
    }
}
