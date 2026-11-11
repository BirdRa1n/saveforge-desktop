import QtQuick
import QtQuick.Layouts

// App mark (accent tile) and name, sized for the 32 px title bar.
RowLayout {
    spacing: 7

    Rectangle {
        Layout.preferredWidth: 18
        Layout.preferredHeight: 18
        radius: 5
        color: Theme.accent

        Icon {
            anchors.centerIn: parent
            width: 12
            height: 12
            strokeWidth: 2.4
            path: Icons.vault
            color: Theme.textOnAccent
        }
    }

    Text {
        text: "SaveVault"
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 14
        font.weight: Font.DemiBold
    }
}
