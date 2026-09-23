// Encabezado ordenable para tablas (mejora #7).
// Uso: SortHeader { label: "Nombre"; active: sortKey==="name"; asc: sortAsc; onClicked: setSort("name") }
import QtQuick
import QtQuick.Controls

Button {
    property string label: ""
    property bool active: false
    property bool asc: true

    text: label + (active ? (asc ? " ▲" : " ▼") : "")
    flat: true
    font.bold: active
    implicitHeight: 40
    Accessible.name: qsTr("Ordenar por %1").arg(label)
}
