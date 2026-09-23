// Tarjeta de indicador para el tablero.
// Mejora #6: icono + delta comparativo (sin romper title/value existentes).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtSalesSystem

Pane {
    property string title
    property string value: ""
    property string icon: ""
    property string delta: ""
    property bool deltaUp: true
    property bool alert: false

    Material.elevation: 2
    padding: Theme.marginMedium

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacingMedium
        Label {
            visible: icon !== ""
            text: icon
            font.pixelSize: Theme.fontDisplay
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: title
                font.pixelSize: Theme.fontS
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: value
                font.pixelSize: Theme.fontXL
                font.bold: true
                color: alert ? Theme.error : palette.text
            }
            Label {
                visible: delta !== ""
                text: delta
                font.pixelSize: Theme.fontS
                font.bold: true
                color: deltaUp ? Theme.success : Theme.error
            }
        }
    }
}
