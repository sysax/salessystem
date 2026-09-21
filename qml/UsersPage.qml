// Usuarios, roles y 2FA (antes users.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    RowLayout {
        Label {
            text: qsTr("Usuarios y roles")
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Nuevo")
            onClicked: addDialog.open()
        }
    }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: usersCtl.users
        delegate: RowLayout {
            width: ListView.view.width
            Label {
                text: modelData.username + "  ·  " + modelData.role + (modelData.totpEnabled ? "  ·  2FA" : "")
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: modelData.active ? 1.0 : 0.5
            }
            Button {
                text: modelData.active ? qsTr("Bloquear") : qsTr("Activar")
                onClicked: {
                    var r = usersCtl.setActive(modelData.username, !modelData.active);
                    if (!r.ok)
                        msg.text = r.error;
                }
            }
            Button {
                text: qsTr("Clave")
                onClicked: {
                    resetDialog.username = modelData.username;
                    resetDialog.open();
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
    Label {
        id: msg
        color: "red"
    }

    Dialog {
        id: addDialog
        title: qsTr("Nuevo usuario")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            TextField {
                id: uName
                placeholderText: qsTr("Usuario")
            }
            TextField {
                id: uPass
                placeholderText: qsTr("Clave (mín. 4)")
                echoMode: TextInput.Password
            }
            ComboBox {
                id: uRole
                model: ["Administrador", "Vendedor", "Cajero", "Almacén", "Contador"]
            }
            Label {
                id: uErr
                color: "red"
            }
        }
        onAccepted: {
            var r = usersCtl.add(uName.text, uPass.text, uRole.currentText);
            if (!r.ok) {
                uErr.text = r.error;
                open();
            }
        }
    }
    Dialog {
        id: resetDialog
        title: qsTr("Nueva clave para ") + username
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string username: ""
        ColumnLayout {
            TextField {
                id: rPass
                placeholderText: qsTr("Nueva clave")
                echoMode: TextInput.Password
            }
        }
        onAccepted: {
            var r = usersCtl.resetPassword(resetDialog.username, rPass.text);
            if (!r.ok)
                msg.text = r.error;
        }
    }
}
