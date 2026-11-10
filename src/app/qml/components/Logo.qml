import QtQuick
import QtQuick.Layouts

// App mark (accent tile) and name.
RowLayout {
    spacing: Theme.space2

    Rectangle {
        Layout.preferredWidth: 22
        Layout.preferredHeight: 22
        radius: 6
        color: Theme.accent

        Icon {
            anchors.centerIn: parent
            width: 14
            height: 14
            strokeWidth: 2.2
            path: Icons.vault
            color: Theme.textOnAccent
        }
    }

    Text {
        text: "SaveVault"
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 16
        font.weight: Font.Bold
    }
}
