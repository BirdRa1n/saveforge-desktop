import QtQuick
import QtQuick.Layouts

// Estado vazio: ícone, título, descrição e ações, centralizados na área que o
// componente ocupa (anchors.fill ou Layout.fill*). O conteúdo fica num bloco
// centralizado para que a largura do texto nunca desloque o centro.
Item {
    id: root

    property string iconPath: Icons.gamepad
    property string title
    property string description
    default property alias actions: actionRow.data

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content
        anchors.centerIn: parent
        width: Math.min(root.width, 480)
        spacing: Theme.space3

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
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            text: root.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textSectionTitle
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
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
    }
}
