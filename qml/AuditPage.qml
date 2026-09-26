// Fase 5: explorador de bitácora (solo Administrador).
// Responde "¿quién cambió este precio y cuándo?" (antes/después).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtSalesSystem

ColumnLayout {
    id: root
    spacing: Theme.spacingSmall

    RowLayout {
        Label {
            text: qsTr("Bitácora de auditoría")
            font.pixelSize: Theme.fontL
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Actualizar")
            implicitHeight: 48
            onClicked: auditCtl.search(filterField.text, userField.text, "")
        }
    }
    RowLayout {
        TextField {
            id: filterField
            placeholderText: qsTr("Buscar (acción, detalle, entidad)…")
            Layout.fillWidth: true
            implicitHeight: 40
            onAccepted: auditCtl.search(text, userField.text, "")
        }
        TextField {
            id: userField
            placeholderText: qsTr("Usuario (vacío = todos)")
            Layout.preferredWidth: 180
            implicitHeight: 40
            onAccepted: auditCtl.search(filterField.text, text, "")
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: auditCtl.entries
        delegate: ColumnLayout {
            width: ListView.view.width
            spacing: 2
            Label {
                text: modelData.ts + "  ·  " + modelData.user + "  ·  " + modelData.action
                      + (modelData.entityId !== "" ? "  ·  " + modelData.entity + ":" + modelData.entityId : "")
                font.bold: true
                font.pixelSize: Theme.fontM
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label {
                visible: (modelData.detail || "") !== ""
                text: modelData.detail
                font.pixelSize: Theme.fontS
                opacity: 0.8
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Label {
                visible: (modelData.before || "{}") !== "{}"
                text: "− " + modelData.before + "\n+ " + modelData.after
                font.pixelSize: Theme.fontS
                font.family: "monospace"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                opacity: 0.2
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Component.onCompleted: auditCtl.search("", "", "")
}
