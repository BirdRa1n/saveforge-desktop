import QtQuick
import QtQuick.Layouts

// Transient message in the bottom-right corner. Call show(text, kind) with kind
// "success", "warning", "error" or "info".
Rectangle {
    id: root

    property string kind: "info"
    property alias text: label.text

    function show(message, messageKind) {
        label.text = message
        kind = messageKind
        visible = true
        opacity = 1
        hideTimer.restart()
    }

    visible: false
    opacity: 0
    width: Math.min(440, layout.implicitWidth + 2 * Theme.space4)
    height: layout.implicitHeight + 2 * Theme.space3
    radius: Theme.radiusControl
    color: Theme.surfaceRaised
    border.color: kind === "error" ? "#F87171" : kind === "warning" ? Theme.warningFg
                : kind === "success" ? Theme.successFg : Theme.borderStrong
    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Behavior on opacity { NumberAnimation { duration: 180 } }
    onOpacityChanged: if (opacity === 0) visible = false

    Timer {
        id: hideTimer
        interval: 4500
        onTriggered: root.opacity = 0
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.space3
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        spacing: Theme.space3

        Icon {
            path: root.kind === "error" || root.kind === "warning" ? Icons.warning : Icons.check
            color: root.kind === "info" ? Theme.textMuted : root.border.color
        }
        Text {
            id: label
            Layout.fillWidth: true
            Layout.maximumWidth: 380
            color: Theme.text
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            wrapMode: Text.Wrap
        }
    }
}
