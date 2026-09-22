// Estado vacío reutilizable (mejora #8): guía cuando una lista no tiene datos.
// Uso: EmptyState { icon: "📦"; title: qsTr("Sin productos"); hint: qsTr("..."); actionText: qsTr("Nuevo"); onAction: ... }
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string icon: "📭"
    property string title: qsTr("Sin datos")
    property string hint: ""
    property string actionText: ""

    signal action()

    spacing: 6

    Label {
        text: root.icon
        font.pixelSize: 36
        Layout.alignment: Qt.AlignHCenter
        opacity: 0.8
    }
    Label {
        text: root.title
        font.bold: true
        font.pixelSize: 15
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
    Label {
        visible: root.hint !== ""
        text: root.hint
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        opacity: 0.7
    }
    Button {
        visible: root.actionText !== ""
        text: root.actionText
        implicitHeight: 44
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.action()
    }
}
