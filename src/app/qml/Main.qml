import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "SaveForge"
    color: Theme.bg

    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            objectName: "sidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            vaultPath: qsTr("Não configurado")
            statusText: qsTr("Funciona offline")
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentIndex

            LibraryPage {
                objectName: "libraryPage"
            }
            PlaceholderPage {
                title: qsTr("Atividade")
                iconPath: Icons.activity
                description: qsTr("Aqui aparecerá o histórico de backups, restaurações e verificações.")
            }
            PlaceholderPage {
                title: qsTr("Importar / Exportar")
                iconPath: Icons.transfer
                description: qsTr("Aqui você poderá exportar seus saves em um arquivo .gamesave e importá-los em outro computador.")
            }
            PlaceholderPage {
                title: qsTr("Configurações")
                iconPath: Icons.settings
                description: qsTr("Aqui ficarão o diretório do vault, a retenção de backups e a política de cloud dos launchers.")
            }
        }
    }
}
