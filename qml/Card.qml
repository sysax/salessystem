// Tarjeta de indicador para el tablero.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Pane {
    property string title
    property string value: ""

    Material.elevation: 2
    Label {
        text: title
        font.pixelSize: 12
        opacity: 0.7
    }
    Label {
        anchors.top: parent.top
        anchors.topMargin: 18
        text: value
        font.pixelSize: 20
        font.bold: true
    }
}
