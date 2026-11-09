import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string iconPath: Icons.gamepad
    property string title
    property string description
    default property alias actions: actionRow.data

    spacing: Theme.space3

    // Espaçadores mantêm o conteúdo centralizado na vertical.
    Item { Layout.fillHeight: true }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 64
        Layout.preferredHeight: 64
        radius: 16
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong

        Icon {
            anchors.centerIn: parent
            width: 28
            height: 28
            path: root.iconPath
            color: Theme.accent
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.space2
        text: root.title
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: Theme.textSectionTitle
        font.weight: Font.Bold
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        text: root.description
        color: Theme.textMuted
        font.family: Theme.fontBody
        font.pixelSize: Theme.textBody
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        lineHeight: 1.4
    }

    RowLayout {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.space3
        spacing: Theme.space3
    }

    Item { Layout.fillHeight: true }
}
