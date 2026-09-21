// Tablero estilo GesNet: tarjetas + comparativa 7 días (antes dashboard.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    signal go(string screen)

    ColumnLayout {
        width: root.width - 24
        spacing: 12

        RowLayout {
            Label {
                text: qsTr("Tablero")
                font.pixelSize: 22
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Actualizar")
                onClicked: dash.refresh()
            }
        }

        GridLayout {
            columns: 3
            Layout.fillWidth: true

            Repeater {
                model: [
                    { "t": qsTr("Ventas pagadas"), "v": money(dash.data.totalSales) },
                    { "t": qsTr("Productos"), "v": dash.data.totalProducts },
                    { "t": qsTr("Clientes"), "v": dash.data.totalClients },
                    { "t": qsTr("Pendientes"), "v": dash.data.pendingOrders },
                    { "t": qsTr("Stock bajo"), "v": dash.data.lowStockAlerts },
                    { "t": qsTr("Conversión"), "v": (dash.data.kpis ? dash.data.kpis.conversion.toFixed(1) + " %" : "") },
                ]
                Card {
                    Layout.fillWidth: true
                    title: modelData.t
                    value: modelData.v
                }
            }
        }

        Label {
            text: qsTr("Últimos 7 días")
            font.bold: true
        }
        Repeater {
            model: dash.data.salesByDay || []
            delegate: RowLayout {
                width: parent.width
                Label {
                    text: modelData.day
                    Layout.preferredWidth: 60
                }
                ProgressBar {
                    Layout.fillWidth: true
                    value: maxDay() > 0 ? modelData.total / maxDay() : 0
                }
                Label {
                    text: money(modelData.total)
                    Layout.preferredWidth: 120
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        Label {
            text: qsTr("Más vendidos")
            font.bold: true
        }
        Repeater {
            model: dash.data.topProducts || []
            delegate: Label {
                text: "• " + modelData.name + "  (" + modelData.sold + ")"
                width: parent.width
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
    function maxDay() {
        var m = 0;
        var arr = dash.data.salesByDay || [];
        for (var i = 0; i < arr.length; ++i)
            if (arr[i].total > m)
                m = arr[i].total;
        return m;
    }
}
