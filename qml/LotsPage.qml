// Fase 4: Lotes y vencimientos (farmacia, veterinaria, abarrotes...).
// Lista próximos a vencer + exportación CSV.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem
import "components"

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    property int horizon: 30

    RowLayout {
        Label {
            text: qsTr("Lotes y vencimientos")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        ComboBox {
            id: horizonBox
            model: [30, 60, 90]
            currentIndex: 0
            onCurrentIndexChanged: root.horizon = currentValue
        }
        Button {
            text: qsTr("CSV")
            onClicked: msg.text = reports.exportCsv("vencimientos", "/tmp")
        }
    }
    Label {
        id: msg
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: reports.expiringProducts(root.horizon)
        visible: count > 0
        delegate: ItemDelegate {
            width: ListView.view.width
            height: 48
            contentItem: RowLayout {
                spacing: Theme.spacingSmall
                Label {
                    text: modelData.name
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontM
                }
                Label {
                    text: qsTr("lote %1").arg(modelData.lote || "—")
                    font.pixelSize: Theme.fontS
                    opacity: 0.7
                }
                Label {
                    text: modelData.vencimiento
                    font.pixelSize: Theme.fontS
                    font.bold: true
                    color: Theme.warning
                }
                Label {
                    text: qsTr("stock %1").arg(modelData.stock)
                    font.pixelSize: Theme.fontS
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: reports.expiringProducts(root.horizon).length === 0
        icon: "📅"
        title: qsTr("Sin vencimientos próximos")
        hint: qsTr("Nada vence en los próximos %1 días.").arg(root.horizon)
    }
}
