import QtQuick
import QtQuick.Layouts

RowLayout {
    spacing: 10

    Icon {
        Layout.preferredWidth: 28
        Layout.preferredHeight: 28
        path: Icons.anvil
        color: Theme.accent
    }

    Text {
        text: "SaveForge"
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 20
        font.weight: Font.Bold
        font.letterSpacing: -0.4
    }
}
