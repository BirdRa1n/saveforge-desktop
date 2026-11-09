import QtQuick
import QtQuick.Layouts

// Página ainda não implementada: título + estado vazio.
Item {
    id: root

    property string title
    property string description
    property string iconPath: Icons.gamepad

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space8
        anchors.topMargin: 28
        spacing: 22

        Text {
            text: root.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textPageTitle
            font.weight: Font.Bold
            font.letterSpacing: -0.6
            Accessible.role: Accessible.Heading
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconPath: root.iconPath
            title: qsTr("Em construção")
            description: root.description
        }
    }
}
