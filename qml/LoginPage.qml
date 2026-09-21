// Login + 2FA (antes screens/login.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    signal loggedIn

    property bool needTotp: false

    ColumnLayout {
        anchors.centerIn: parent
        width: 320
        spacing: 10

        Label {
            text: qsTr("Sistema de Ventas")
            font.pixelSize: 26
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        TextField {
            id: userField
            placeholderText: qsTr("Usuario")
            Layout.fillWidth: true
        }
        TextField {
            id: passField
            placeholderText: qsTr("Clave")
            echoMode: TextInput.Password
            Layout.fillWidth: true
            onAccepted: doLogin()
        }
        TextField {
            id: totpField
            visible: root.needTotp
            placeholderText: qsTr("Código 2FA")
            Layout.fillWidth: true
            onAccepted: doTotp()
        }
        Label {
            id: errLabel
            color: "red"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Button {
            text: root.needTotp ? qsTr("Verificar 2FA") : qsTr("Entrar")
            highlighted: true
            Layout.fillWidth: true
            onClicked: root.needTotp ? doTotp() : doLogin()
        }
        Label {
            text: qsTr("admin / admin123 · vendedor / venta123 · cajero / caja123")
            font.pixelSize: 11
            opacity: 0.6
            Layout.alignment: Qt.AlignHCenter
        }
    }

    function doLogin() {
        var r = auth.login(userField.text, passField.text);
        if (r.ok) {
            errLabel.text = "";
            root.loggedIn();
        } else if (r.totpRequired) {
            root.needTotp = true;
            errLabel.text = qsTr("Ingrese su código 2FA");
        } else {
            errLabel.text = r.error;
        }
    }

    function doTotp() {
        var r = auth.verifyTotp(totpField.text);
        if (r.ok) {
            errLabel.text = "";
            root.needTotp = false;
            root.loggedIn();
        } else {
            errLabel.text = r.error;
        }
    }
}
