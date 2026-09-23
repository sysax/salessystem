// Login + 2FA (antes screens/login.py).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Utils.js" as Utils
import QtSalesSystem

Pane {
    id: root
    signal loggedIn

    property bool needTotp: false
    // Cambio forzoso de clave por defecto (primer ingreso): usuario y clave
    // actual pendientes hasta completar el cambio; sin esto no se entra.
    property string pendingChangeUser: ""
    property string pendingCurrentPass: ""

    // Focus management (mejora #10): foco inicial y al pedir 2FA
    onNeedTotpChanged: { if (needTotp) totpField.forceActiveFocus() }
    Component.onCompleted: userField.forceActiveFocus()

    ColumnLayout {
        anchors.centerIn: parent
        width: 320
        spacing: Theme.spacingMedium

        Label {
            text: qsTr("Sistema de Ventas")
            font.pixelSize: Theme.fontDisplay
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
            // No llamar al backend con campos vacíos (el manejo de error vía Toast se conserva)
            enabled: root.needTotp ? totpField.text.trim() !== ""
                                   : userField.text.trim() !== "" && passField.text !== ""
            onClicked: root.needTotp ? doTotp() : doLogin()
        }
        Label {
            text: qsTr("Usuario inicial: admin / admin123 (se pedirá cambio de clave)")
            font.pixelSize: Theme.fontXS
            opacity: 0.6
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Diálogo forzoso e ineludible: no se puede cerrar sin cambiar la clave.
    Dialog {
        id: forcePassDialog
        title: qsTr("Cambie su contraseña")
        modal: true
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Ok
        onOpened: newPassField.forceActiveFocus()
        ColumnLayout {
            Label {
                text: qsTr("Por seguridad debe cambiar la clave por defecto para continuar.")
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 300
            }
            TextField {
                id: newPassField
                placeholderText: qsTr("Nueva clave (mín. 4)")
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }
            TextField {
                id: confirmPassField
                placeholderText: qsTr("Confirmar nueva clave")
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }
            Label {
                id: forcePassErr
                color: Theme.error
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 300
            }
        }
        onAccepted: {
            if (newPassField.text.length < 4) {
                forcePassErr.text = qsTr("Contraseña mínimo 4 caracteres");
                open();
                return;
            }
            if (newPassField.text !== confirmPassField.text) {
                forcePassErr.text = qsTr("Las claves no coinciden");
                open();
                return;
            }
            var r = auth.changePassword(root.pendingChangeUser, root.pendingCurrentPass,
                                        newPassField.text);
            if (!r.ok) {
                forcePassErr.text = r.error;
                open();
                return;
            }
            newPassField.text = "";
            confirmPassField.text = "";
            forcePassErr.text = "";
            passField.text = "";
            root.pendingChangeUser = "";
            root.pendingCurrentPass = "";
            Utils.showToast("success", "¡Bienvenido!", 2000);
            root.loggedIn();
        }
    }

    function askForcedChange(user, currentPass) {
        root.pendingChangeUser = user;
        root.pendingCurrentPass = currentPass;
        forcePassErr.text = "";
        newPassField.text = "";
        confirmPassField.text = "";
        Utils.showToast("warning", "Debe cambiar la clave por defecto", 4000);
        forcePassDialog.open();
    }

    function doLogin() {
        var r = auth.login(userField.text, passField.text);
        if (r.ok) {
            if (r.mustChangePassword) {
                askForcedChange(r.user, passField.text);
            } else {
                Utils.showToast("success", "¡Bienvenido!", 2000);
                root.loggedIn();
            }
        } else if (r.totpRequired) {
            root.needTotp = true;
            Utils.showToast("info", "Ingrese su código 2FA", 3000);
        } else {
            Utils.showToast("error", r.error, 4000);
        }
    }

    function doTotp() {
        var r = auth.verifyTotp(totpField.text);
        if (r.ok) {
            if (r.mustChangePassword) {
                askForcedChange(r.user, passField.text);
            } else {
                Utils.showToast("success", "Autenticación exitosa", 2000);
                root.needTotp = false;
                root.loggedIn();
            }
        } else {
            Utils.showToast("error", r.error, 4000);
        }
    }
}
