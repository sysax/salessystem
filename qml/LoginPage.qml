// Login + 2FA (antes screens/login.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils

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
        Utils.showLoading(qsTr("Ingresando..."));
        var r = auth.login(userField.text, passField.text);
        Utils.hideLoading();
        if (r.ok) {
            Utils.showToast("success", "¡Bienvenido!", 2000);
            root.loggedIn();
        } else if (r.totpRequired) {
            root.needTotp = true;
            Utils.showToast("info", "Ingrese su código 2FA", 3000);
        } else {
            Utils.showToast("error", r.error, 4000);
        }
    }

    function doTotp() {
        Utils.showLoading(qsTr("Verificando..."));
        var r = auth.verifyTotp(totpField.text);
        Utils.hideLoading();
        if (r.ok) {
            Utils.showToast("success", "Autenticación exitosa", 2000);
            root.needTotp = false;
            root.loggedIn();
        } else {
            Utils.showToast("error", r.error, 4000);
        }
    }
}
