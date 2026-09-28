pragma Singleton
// QtSalesSystem — design tokens (mejora #9).
// Fuente única de colores, espaciados, fuentes y constantes de overlay.
// Las páginas migran progresivamente a estos tokens.
import QtQuick

QtObject {
    id: theme

    // Fase 7: modo oscuro / alto contraste / densidad (settings theme,
    // high_contrast, pos_density). Reactive: settingsChanged re-evalúa.
    // `typeof` defensivo: el singleton también se instancia en tests QML
    // aislados sin settingsCtl en el contexto.
    readonly property bool dark: typeof settingsCtl !== "undefined" && settingsCtl
                                  && settingsCtl.settings["theme"] === "dark"
    readonly property bool highContrast: typeof settingsCtl !== "undefined" && settingsCtl
                                         && settingsCtl.settings["high_contrast"] === "1"
    // Altura táctil del POS por densidad (Fase 7: layout de caja configurable;
    // mínimo 44px según criterio de accesibilidad).
    readonly property int posTouchH: {
        var d = "normal";
        try { d = settingsCtl.settings["pos_density"] || "normal"; } catch (e) {}
        return d === "compacto" ? 44 : (d === "amplio" ? 56 : 48);
    }

    // Marca
    readonly property color primary: "#7FC8A9"   // menta pastel
    readonly property color accent: "#5AA9E6"

    // Semánticos (toast, estados, errores).
    // En oscuro se aclaran (guía Material Dark); en alto contraste se
    // refuerzan (oscuros sobre claro, claros sobre oscuro).
    readonly property color info: highContrast ? (dark ? "#82B1FF" : "#0D47A1") : (dark ? "#64B5F6" : "#2196F3")
    readonly property color success: highContrast ? (dark ? "#69F0AE" : "#1B5E20") : (dark ? "#81C784" : "#4CAF50")
    readonly property color warning: highContrast ? (dark ? "#FFD740" : "#E65100") : (dark ? "#FFB74D" : "#FF9800")
    readonly property color error: highContrast ? (dark ? "#FF8A80" : "#B71C1C") : (dark ? "#E57373" : "#F44336")

    // Superficies y texto
    readonly property color textOnColor: "white"
    // Texto oscuro para fondos claros semánticos (contraste en Toast success/warning)
    readonly property color textOnBright: "#1A1A1A"
    readonly property color overlayDim: "#80000000"   // velo 50% negro
    readonly property color textOutline: "#20000000"

    // Espaciado / radios / márgenes
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int marginMedium: 12
    readonly property int radiusMedium: 8

    // Escala tipográfica (cubre los tamaños hoy hardcodeados)
    readonly property int fontXS: 11
    readonly property int fontS: 12
    readonly property int fontM: 14
    readonly property int fontML: 16
    readonly property int fontL: 18
    readonly property int fontXL: 20
    readonly property int fontXXL: 22
    readonly property int fontDisplay: 26

    // Toast
    readonly property int toastDuration: 3000
    readonly property int toastWidthMax: 400
    readonly property int toastBottomMargin: 20
    readonly property int toastAnimIn: 300
    readonly property int toastAnimOut: 300

    // Orden de overlays globales
    readonly property int zToast: 9999
    readonly property int zLoading: 9998
}
