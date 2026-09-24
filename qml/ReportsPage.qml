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
            Button {
                text: qsTr("CSV vencimientos")
                onClicked: msg.text = reports.exportCsv("vencimientos", exportDir())
            }
        }
        RowLayout {
            // Fase 4: reportes por vertical.
            Button {
                text: qsTr("CSV seriales")
                onClicked: msg.text = reports.exportCsv("seriales", exportDir())
            }
            Button {
                text: qsTr("CSV mermas")
                onClicked: msg.text = reports.exportCsv("mermas", exportDir())
            }
            Button {
                text: qsTr("CSV rotación")
                onClicked: msg.text = reports.exportCsv("rotacion", exportDir())
            }
            Button {
                text: qsTr("CSV inventario")
                onClicked: msg.text = reports.exportCsv("inventario", exportDir())
            }
        }
        RowLayout {
            // Fase 5: reportes por vertical (según rubro activo).
            Button {
                text: qsTr("CSV controlados")
                visible: verticalIn(["farmacia", "veterinaria"])
                onClicked: msg.text = reports.exportCsv("controlados", exportDir())
            }
            Button {
                text: qsTr("CSV garantías")
                visible: verticalIn(["celulares", "taller"])
                onClicked: msg.text = reports.exportCsv("garantias", exportDir())
            }
            Button {
                text: qsTr("CSV granel")
                visible: verticalIn(["abarrotes", "restaurante", "panaderia", "cafeteria", "miscelanea"])
                onClicked: msg.text = reports.exportCsv("granel", exportDir())
            }
        }
        Label {
            text: qsTr("Por vencer (30 días): %1").arg(reports.expiringProducts(30).length)
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("Seriales en stock: %1 · en RMA: %2").arg(reports.serialsReport().counts.in_stock).arg(reports.serialsReport().counts.rma)
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("Inventario: %1 uds · costo %2").arg(reports.inventoryValue().units).arg(money(reports.inventoryValue().cost))
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("Controlados vendidos: %1").arg(reports.controlledSales().length)
            wrapMode: Text.Wrap
            visible: verticalIn(["farmacia", "veterinaria"])
        }
        Label {
            text: qsTr("En garantía: %1").arg(reports.warrantyOpen().length)
            wrapMode: Text.Wrap
            visible: verticalIn(["celulares", "taller"])
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
    function verticalIn(list) {
        try {
            var bt = settingsCtl.settings["business_type"] || "";
            return list.indexOf(bt) >= 0;
        } catch (e) {
            return true; // sin settings (tests): mostrar todo
        }
    }
    function exportDir() {
        // Exportar junto a la BD de la app no es visible; usar temporal del sistema
        return "/tmp";
    }
}
