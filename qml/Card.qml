// Tarjeta de indicador para el tablero.
// Mejora #6: icono + delta comparativo (sin romper title/value existentes).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    property string title
    property string value: ""
    property string icon: ""
    property string delta: ""
    property bool deltaUp: true
    property bool alert: false

    Material.elevation: 2
    padding: 12

    RowLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            visible: icon !== ""
            text: icon
            font.pixelSize: 26
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: title
                font.pixelSize: 12
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: value
                font.pixelSize: 20
                font.bold: true
                color: alert ? "#F44336" : palette.text
            }
            Label {
                visible: delta !== ""
                text: delta
                font.pixelSize: 12
                font.bold: true
                color: deltaUp ? "#4CAF50" : "#F44336"
            }
        }
    }
}
