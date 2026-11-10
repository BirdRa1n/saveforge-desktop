pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Navigation rail: one icon per page, with the page name as tooltip and
// accessible name. `currentIndex` is the page shown in Main.qml.
Item {
    id: root

    property int currentIndex: 0

    implicitWidth: 64

    ButtonGroup { id: navGroup }

    ColumnLayout {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Theme.space1
        spacing: 6

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
    }
}
