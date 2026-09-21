// Historial y documentos (antes sales.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        Label {
            text: qsTr("Ventas y documentos")
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Actualizar")
            onClicked: salesCtl.refresh()
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: salesCtl.sales
        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData.id + "  ·  " + modelData.client + "  ·  " + money(modelData.total) + "  ·  " + modelData.status + "  ·  " + modelData.docType
            onClicked: {
                var d = salesCtl.detail(modelData.id);
                detailText.text = d.ok ? JSON.stringify(d, null, 1) : d.error;
                detailDialog.saleId = modelData.id;
                detailDialog.open();
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Dialog {
        id: detailDialog
        title: qsTr("Detalle ") + saleId
        modal: true
        standardButtons: Dialog.Close
        property string saleId: ""
        ColumnLayout {
            ScrollView {
                Layout.preferredWidth: 420
                Layout.preferredHeight: 240
                TextArea {
                    id: detailText
                    readOnly: true
                    wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                Button {
                    text: qsTr("Avanzar a Pagada")
                    onClicked: {
                        var r = salesCtl.advance(detailDialog.saleId, "Pagada", auth.currentUser);
                        if (!r.ok)
                            detailText.text = r.error;
                        else
                            detailDialog.close();
                    }
                }
                Button {
                    text: qsTr("Cancelar venta")
                    onClicked: {
                        var r = salesCtl.cancel(detailDialog.saleId, "UI", auth.currentUser);
                        if (!r.ok)
                            detailText.text = r.error;
                        else
                            detailDialog.close();
                    }
                }
            }
        }
    }

    function money(v) {
        return ApplicationWindow.window.money(v);
    }
}
