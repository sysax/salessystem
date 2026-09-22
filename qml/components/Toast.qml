// Toast notification component - Snackbar/Toast style
// Usage: toast.show("Message", duration, type)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    
    // Properties
    property string message: ""
    property int duration: 3000  // milliseconds
    property string type: "info"  // info, success, warning, error
    property bool toastVisible: false
    
    // Colors by type
    readonly property color colorInfo: "#2196F3"
    readonly property color colorSuccess: "#4CAF50"
    readonly property color colorWarning: "#FF9800"
    readonly property color colorError: "#F44336"
    
    readonly property color currentColor: {
        switch(type) {
            case "success": return colorSuccess;
            case "warning": return colorWarning;
            case "error": return colorError;
            default: return colorInfo;
        }
    }
    
    anchors.fill: parent
    z: 9999  // Ensure it's on top of everything (above LoadingOverlay z 9998)
    clip: true
    
    // Toast container
    Rectangle {
        id: toastRect
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width * 0.9, 400)
        height: toastContent.height + 24
        radius: 8
        color: currentColor
        opacity: 0
        visible: opacity > 0
        
        RowLayout {
            id: toastContent
            anchors.centerIn: parent
            width: parent.width - 24
            spacing: 12
            
            // Icon based on type
            Label {
                text: {
                    switch(type) {
                        case "success": return "✓";
                        case "warning": return "⚠";
                        case "error": return "✕";
                        default: return "ℹ";
                    }
                }
                font.pixelSize: 20
                color: "white"
                Layout.alignment: Qt.AlignVCenter
            }
            
            // Message
            Label {
                text: root.message
                color: "white"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
        
        // Animation (standalone object: avoids value-source + binding conflict on opacity)
        SequentialAnimation {
            id: anim
            NumberAnimation { target: toastRect; property: "opacity"; to: 1.0; duration: 300; easing.type: Easing.OutCubic }
            PauseAnimation { duration: root.duration }
            NumberAnimation { target: toastRect; property: "opacity"; to: 0.0; duration: 300; easing.type: Easing.InCubic }
        }
    }
    
    // Show method
    function show(msg, dur, msgType) {
        if (msg !== undefined) root.message = msg;
        if (dur !== undefined) root.duration = dur;
        if (msgType !== undefined) root.type = msgType;
        
        anim.restart();
    }
    
    // Convenience methods
    function success(msg, dur) { show(msg, dur, "success"); }
    function error(msg, dur) { show(msg, dur, "error"); }
    function warning(msg, dur) { show(msg, dur, "warning"); }
    function info(msg, dur) { show(msg, dur, "info"); }
}
