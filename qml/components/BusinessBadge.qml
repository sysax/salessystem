// Fase 4: insignia del rubro activo (nombre + tipo de negocio).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 8

    property string businessName: ""
    property string businessType: ""

    Label {
        text: "🏪"
        font.pixelSize: 16
    }
    Label {
        text: root.businessName !== "" ? root.businessName : qsTr("Mi Negocio")
        font.bold: true
        font.pixelSize: 14
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
    Label {
        visible: root.businessType !== ""
        text: root.businessType
        font.pixelSize: 12
        opacity: 0.7
        padding: 4
        background: Rectangle {
            radius: 4
            opacity: 0.15
        }
    }

    function refresh() {
        try {
            root.businessName = settingsCtl.settings["business_name"] || "";
            root.businessType = settingsCtl.settings["business_type"] || "";
        } catch (e) {}
    }

    Component.onCompleted: refresh()
}
