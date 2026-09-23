// Reportes operativos/financieros/KPIs + exportar (antes reports.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem

ScrollView {
    id: root

    ColumnLayout {
        width: root.width - 24
        spacing: Theme.spacingMedium

        RowLayout {
            Label {
                text: qsTr("Reportes")
                font.pixelSize: Theme.fontXL
                font.bold: true
                Layout.fillWidth: true
            }
            ComboBox {
                id: rangeBox
                model: ["dia", "semana", "mes", "año"]
                onCurrentTextChanged: periodLabel.text = qsTr("Ventas %1: %2 (%3 docs)").arg(currentText).arg(money(reports.salesForPeriod(currentText).total)).arg(reports.salesForPeriod(currentText).count)
            }
        }
        Label {
            id: periodLabel
            text: ""
        }
        GridLayout {
            columns: 3
            Layout.fillWidth: true
            Repeater {
                model: [
                    { "t": qsTr("Ticket promedio"), "v": money(reports.averageTicket()) },
                    { "t": qsTr("Rotación"), "v": reports.kpis().rotacion.toFixed(2) },
                    { "t": qsTr("Conversión"), "v": reports.kpis().conversion.toFixed(1) + " %" },
                    { "t": qsTr("Margen bruto"), "v": reports.kpis().margen_bruto_pct.toFixed(1) + " %" },
                    { "t": qsTr("Flujo neto"), "v": money(reports.cashFlow().neto) },
                    { "t": qsTr("IVA generado"), "v": money(reports.taxes().total) },
                ]
                Card {
                    Layout.fillWidth: true
                    title: modelData.t
                    value: modelData.v
                }
            }
        }
        Label {
            text: qsTr("Estado de resultados")
            font.bold: true
        }
        Label {
            text: qsTr("Ingresos %1 · Costo %2 · Bruto %3 · Neto %4").arg(money(reports.incomeStatement().ingresos)).arg(money(reports.incomeStatement().costo)).arg(money(reports.incomeStatement().bruto)).arg(money(reports.incomeStatement().neto))
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Button {
                text: qsTr("Exportar CSV")
                onClicked: msg.text = reports.exportCsv("operativo", exportDir())
            }
            Button {
                text: qsTr("Exportar PDF")
                onClicked: msg.text = reports.exportPdf("operativo", exportDir())
            }
        }
        Label {
            id: msg
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
    function exportDir() {
        // Exportar junto a la BD de la app no es visible; usar temporal del sistema
        return "/tmp";
    }
}
