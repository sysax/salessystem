// Loading overlay with spinner
// Usage: loadingOverlay.show() / loadingOverlay.hide()
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    
    property bool loadingVisible: false
    property string message: "Cargando..."
    
    anchors.fill: parent
    z: 999  // Just below Toast
    
    // Semi-transparent background
    Rectangle {
        anchors.fill: parent
        color: "#80000000"  // 50% black
        opacity: root.loadingVisible ? 0.5 : 0
        visible: opacity > 0
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }
    
    // Centered content
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: root.loadingVisible
        opacity: root.loadingVisible ? 1 : 0
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        // Spinner using BusyIndicator
        BusyIndicator {
            id: spinner
            running: root.loadingVisible
            Layout.alignment: Qt.AlignHCenter
            width: 48
            height: 48
        }
        
        // Message
        Label {
            text: root.message
            color: "white"
            font.pixelSize: 16
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            style: Text.Outline
            styleColor: "#20000000"
        }
    }
    
    // Show method
    function show(msg) {
        if (msg !== undefined) root.message = msg;
        root.loadingVisible = true;
    }
    
    // Hide method
    function hide() {
        root.loadingVisible = false;
    }
}
