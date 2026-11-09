pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property int currentIndex: 0
    property string vaultPath
    property string statusText

    implicitWidth: 240
    color: Theme.surface

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ButtonGroup { id: navGroup }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        anchors.topMargin: Theme.space6
        spacing: 6

        Logo {
            Layout.leftMargin: Theme.space2
            Layout.bottomMargin: Theme.space5
        }

        Repeater {
            model: [
                { label: qsTr("Biblioteca"), icon: Icons.library },
                { label: qsTr("Atividade"), icon: Icons.activity },
                { label: qsTr("Importar / Exportar"), icon: Icons.transfer },
                { label: qsTr("Configurações"), icon: Icons.settings }
            ]
            delegate: NavButton {
                required property var modelData
                required property int index
                text: modelData.label
                iconPath: modelData.icon
                checked: root.currentIndex === index
                ButtonGroup.group: navGroup
                onClicked: root.currentIndex = index
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: vaultInfo.implicitHeight + 2 * Theme.space3 + 4
            radius: Theme.radiusControl + 2
            color: Theme.surfaceRaised
            border.color: Theme.borderStrong

            ColumnLayout {
                id: vaultInfo
                anchors.fill: parent
                anchors.margins: Theme.space3 + 2
                spacing: 6

                Text {
                    text: qsTr("Vault local")
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: root.vaultPath
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
                Text {
                    Layout.fillWidth: true
                    text: root.statusText
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
