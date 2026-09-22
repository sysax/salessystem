// Diálogo de confirmación reutilizable (mejora #11).
// Responsivo: tope de ancho/alto según ventana + scrollbar si el mensaje es largo.
// Seguro: el foco inicial va a Cancelar para no confirmar con Enter por accidente.
// Uso: ConfirmDialog { message: "..."; confirmText: qsTr("Sí, eliminar"); onAccepted: {...} }
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem

Dialog {
    id: root

    property string message: ""
    property string confirmText: qsTr("Confirmar")
    property bool danger: true

    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    // Tope responsivo: nunca más ancho que la ventana con margen
    width: Math.min(400, (ApplicationWindow.window ? ApplicationWindow.window.width : 480) - 48)

    onOpened: standardButton(Dialog.Cancel).forceActiveFocus()

    Component.onCompleted: {
        standardButton(Dialog.Ok).text = Qt.binding(function() { return root.confirmText; });
        standardButton(Dialog.Ok).highlighted = Qt.binding(function() { return root.danger; });
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingSmall
        RowLayout {
            spacing: Theme.spacingSmall
            Label {
                text: "⚠️"
                font.pixelSize: Theme.fontXL
                Layout.alignment: Qt.AlignTop
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(msgLabel.implicitHeight + 8, 180)
                clip: true
                Label {
                    id: msgLabel
                    text: root.message
                    wrapMode: Text.WordWrap
                    width: parent.width
                    font.pixelSize: Theme.fontM
                }
            }
        }
    }
}
