import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

ApplicationWindow {
    id: window

    // Game shown in the detail page; empty shows the selected sidebar page.
    property string selectedGameId

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "SaveForge"
    color: Theme.bg

    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody

    Connections {
        target: AppController
        function onNotify(text, kind) { toast.show(text, kind) }
    }

    // With a tray icon and the user's consent, closing only hides the window
    // so automatic backups keep running; otherwise it quits the app.
    onClosing: close => {
        if (AppController.trayAvailable && AppController.runInBackground) {
            close.accepted = false
            window.hide()
        } else {
            Qt.quit()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            objectName: "sidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            vaultPath: AppController.vaultReady ? AppController.vaultPath : qsTr("Indisponível")
            statusText: AppController.busy ? AppController.busyText
                                           : Format.count(AppController.library.count, qsTr("jogo"), qsTr("jogos")) + qsTr(" · funciona offline")
            onCurrentIndexChanged: window.selectedGameId = ""
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.selectedGameId.length > 0 ? 4 : sidebar.currentIndex

            LibraryPage {
                objectName: "libraryPage"
                onGameSelected: gameId => window.selectedGameId = gameId
                onOpenSettings: sidebar.currentIndex = 3
            }
            ActivityPage {
                objectName: "activityPage"
            }
            TransferPage {
                objectName: "transferPage"
            }
            SettingsPage {
                objectName: "settingsPage"
            }
            GameDetailPage {
                objectName: "gameDetailPage"
                gameId: window.selectedGameId
                onBack: window.selectedGameId = ""
            }
        }
    }

    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space6
    }
}
