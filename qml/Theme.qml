pragma Singleton
// QtSalesSystem — design tokens (mejora #9).
// Fuente única de colores, espaciados, fuentes y constantes de overlay.
// Las páginas migran progresivamente a estos tokens.
import QtQuick

QtObject {
    id: theme

    // Marca
    readonly property color primary: "#7FC8A9"   // menta pastel
    readonly property color accent: "#5AA9E6"

    // Semánticos (toast, estados, errores)
    readonly property color info: "#2196F3"
    readonly property color success: "#4CAF50"
    readonly property color warning: "#FF9800"
    readonly property color error: "#F44336"

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
