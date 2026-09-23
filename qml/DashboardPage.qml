// Tablero estilo GesNet: tarjetas + comparativa 7 días (antes dashboard.py).
// Mejora #6 Dashboard visual: iconos en tarjetas, comparativas %, gráfico de
// barras propio (sin Qt Charts para no añadir dependencia/CI), ranking y alertas.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem

ScrollView {
    id: root
    signal go(string screen)

    ColumnLayout {
        width: root.width - 24
        spacing: Theme.spacingSmall

        RowLayout {
            spacing: Theme.spacingSmall
            Label {
                text: qsTr("Tablero")
                font.pixelSize: Theme.fontXXL
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                visible: weekDelta() !== ""
                text: weekDelta()
                font.pixelSize: Theme.fontM
                font.bold: true
                color: dayDeltaUp() ? Theme.success : Theme.error
            }
            Button {
                text: qsTr("Actualizar")
                implicitHeight: 48
                onClicked: dash.refresh()
            }
        }

        // Fila 1: KPIs operativos con iconos + comparativa hoy vs ayer en ventas
        GridLayout {
            columns: 3
            columnSpacing: Theme.spacingSmall
            rowSpacing: Theme.spacingSmall
            Layout.fillWidth: true

            Card {
                Layout.fillWidth: true
                title: qsTr("Ventas pagadas")
                icon: "💰"
                value: money(dash.data.totalSales)
                delta: dayDeltaText()
                deltaUp: dayDeltaUp()
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Productos")
                icon: "📦"
                value: String(dash.data.totalProducts || 0)
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Clientes")
                icon: "👥"
                value: String(dash.data.totalClients || 0)
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Pendientes")
                icon: "⏳"
                value: String(dash.data.pendingOrders || 0)
                alert: (dash.data.pendingOrders || 0) > 0
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Stock bajo")
                icon: "⚠️"
                value: String(dash.data.lowStockAlerts || 0)
                alert: (dash.data.lowStockAlerts || 0) > 0
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Conversión")
                icon: "📈"
                value: dash.data.kpis ? dash.data.kpis.conversion.toFixed(1) + " %" : ""
            }
        }

        // Fila 2: KPIs financieros
        GridLayout {
            columns: 3
            columnSpacing: Theme.spacingSmall
            rowSpacing: Theme.spacingSmall
            Layout.fillWidth: true

            Card {
                Layout.fillWidth: true
                title: qsTr("Margen bruto")
                icon: "💹"
                value: dash.data.kpis ? dash.data.kpis.margen_bruto_pct.toFixed(1) + " %" : ""
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Margen neto")
                icon: "🧮"
                value: dash.data.kpis ? dash.data.kpis.margen_neto_pct.toFixed(1) + " %" : ""
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("Rotación inv.")
                icon: "🔄"
                value: dash.data.kpis ? dash.data.kpis.rotacion.toFixed(2) + "×" : ""
                delta: dash.data.kpis ? qsTr("%1 días").arg(Math.round(dash.data.kpis.dias_inventario)) : ""
                deltaUp: true
            }
        }

        GroupBox {
            title: qsTr("Ventas últimos 7 días · Total %1").arg(money(weekTotal()))
            Layout.fillWidth: true
            // Gráfico de barras sin dependencias: 7 columnas proporcionales al máximo
            RowLayout {
                anchors.fill: parent
                spacing: 6
                Repeater {
                    model: dash.data.salesByDay || []
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBottom
                        spacing: 4
                        Label {
                            text: money(modelData.total)
                            font.pixelSize: Theme.fontXS
                            Layout.alignment: Qt.AlignHCenter
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: barH(modelData.total)
                            radius: 4
                            color: index === 6 ? Theme.accent : Theme.primary
                            border.color: Theme.textOutline
                        }
                        Label {
                            text: modelData.day
                            font.pixelSize: Theme.fontXS
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }

        RowLayout {
            spacing: Theme.spacingSmall
            Label {
                text: qsTr("Más vendidos")
                font.bold: true
                font.pixelSize: Theme.fontML
                Layout.fillWidth: true
            }
            ToolButton {
                text: qsTr("Ver reportes →")
                Accessible.name: qsTr("Ir a reportes")
                onClicked: root.go("reports")
            }
        }
        Repeater {
            model: dash.data.topProducts || []
            delegate: RowLayout {
                width: parent.width
                spacing: Theme.spacingSmall
                Label {
                    text: medal(index)
                    font.pixelSize: Theme.fontML
                    Layout.preferredWidth: 30
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    text: modelData.name
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ProgressBar {
                    Layout.preferredWidth: 140
                    value: topMax() > 0 ? modelData.sold / topMax() : 0
                }
                Label {
                    text: String(modelData.sold)
                    Layout.preferredWidth: 50
                    horizontalAlignment: Text.AlignRight
                    font.bold: true
                }
            }
        }

        RowLayout {
            spacing: Theme.spacingSmall
            visible: (dash.data.lowStock || []).length > 0
            Label {
                text: qsTr("⚠️ Stock bajo (%1)").arg((dash.data.lowStock || []).length)
                font.bold: true
                font.pixelSize: Theme.fontML
                Layout.fillWidth: true
            }
            ToolButton {
                text: qsTr("Ver inventario →")
                Accessible.name: qsTr("Ir a inventario")
                onClicked: root.go("inventory")
            }
        }
        Repeater {
            model: dash.data.lowStock || []
            delegate: Label {
                visible: index < 5
                text: "• " + modelData.name + "  (" + modelData.stock + "/" + modelData.min + ")"
                width: parent.width
                elide: Text.ElideRight
                opacity: 0.85
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
    function barH(total) {
        var m = maxDay();
        if (m <= 0)
            return 8;
        return 20 + Math.round(total / m * 110);
    }
    function weekTotal() {
        var s = 0;
        var arr = dash.data.salesByDay || [];
        for (var i = 0; i < arr.length; ++i)
            s += arr[i].total;
        return s;
    }
    function dayDeltaText() {
        var arr = dash.data.salesByDay || [];
        if (arr.length < 2)
            return "";
        var hoy = arr[arr.length - 1].total;
        var ayer = arr[arr.length - 2].total;
        if (ayer <= 0)
            return hoy > 0 ? qsTr("▲ nuevo") : "";
        var p = (hoy - ayer) / ayer * 100;
        return (p >= 0 ? "▲ +" : "▼ ") + p.toFixed(1) + " %";
    }
    function dayDeltaUp() {
        var arr = dash.data.salesByDay || [];
        if (arr.length < 2)
            return true;
        return arr[arr.length - 1].total >= arr[arr.length - 2].total;
    }
    function weekDelta() {
        var t = dayDeltaText();
        return t === "" ? "" : qsTr("Hoy vs ayer: %1").arg(t);
    }
    function topMax() {
        var m = 0;
        var arr = dash.data.topProducts || [];
        for (var i = 0; i < arr.length; ++i)
            if (arr[i].sold > m)
                m = arr[i].sold;
        return m;
    }
    function medal(i) {
        if (i === 0)
            return "🥇";
        if (i === 1)
            return "🥈";
        if (i === 2)
            return "🥉";
        return String(i + 1) + ".";
    }
}
