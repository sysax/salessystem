// Controles de paginación reutilizables (mejora #7).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 4

    property int page: 0
    property int pageCount: 1
    property int total: 0
    property int pageSize: 20

    signal first()
    signal prev()
    signal next()
    signal last()
    signal sizeChanged(int size)

    ComboBox {
        id: sizeBox
        model: [10, 20, 50]
        currentIndex: 1
        implicitHeight: 40
        Accessible.name: qsTr("Filas por página")
        onActivated: root.sizeChanged(currentValue)
    }
    Button {
        text: "«"
        implicitHeight: 40
        implicitWidth: 44
        enabled: root.page > 0
        Accessible.name: qsTr("Primera página")
        onClicked: root.first()
    }
    Button {
        text: "‹"
        implicitHeight: 40
        implicitWidth: 44
        enabled: root.page > 0
        Accessible.name: qsTr("Página anterior")
        onClicked: root.prev()
    }
    Label {
        text: qsTr("Pág %1/%2 · %3").arg(root.page + 1).arg(Math.max(root.pageCount, 1)).arg(root.total)
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
    Button {
        text: "›"
        implicitHeight: 40
        implicitWidth: 44
        enabled: root.page < root.pageCount - 1
        Accessible.name: qsTr("Página siguiente")
        onClicked: root.next()
    }
    Button {
        text: "»"
        implicitHeight: 40
        implicitWidth: 44
        enabled: root.page < root.pageCount - 1
        Accessible.name: qsTr("Última página")
        onClicked: root.last()
    }
}
