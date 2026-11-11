import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveVault.Backend

ApplicationWindow {
    id: window

    // Game selected in the library (shown next to the list); setting it
    // also brings the library forward.
    property string selectedGameId
    // Windows and macOS let the content extend into the title bar while the
    // system keeps drawing its own window buttons (Qt 6.9+), so the app's
    // title bar holds the logo and the search (see TitleBar.qml and ADR 0006).
    // Linux desktops keep their usual decorations.
    readonly property bool integratedTitleBar: Qt.platform.os === "windows" || Qt.platform.os === "osx"
    readonly property bool onLibrary: sidebar.currentIndex === 0
    readonly property bool canActOnGame: onLibrary && selectedGameId.length > 0

    onSelectedGameIdChanged: if (selectedGameId.length > 0) sidebar.currentIndex = 0

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    // macOS would draw the title over the search; the Dock and the menu bar
    // still show the app name from the bundle.
    title: Qt.platform.os === "osx" ? "" : "SaveVault"
    color: Theme.window
    // On Windows, leaving out Qt.WindowTitleHint stops Qt from drawing its own
    // icon and title in the caption area; the buttons remain native.
    flags: !integratedTitleBar ? Qt.Window
         : Qt.platform.os === "windows"
           ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint | Qt.CustomizeWindowHint
             | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
           : Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint

    // ApplicationWindow keeps its content inside the safe area by default;
    // the title bar fills that area itself.
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody

    Connections {
        target: AppController
        function onNotify(text, kind) { toast.show(text, kind) }
        function onLargeBackupFound(gameId, name, size, expected) {
            largeBackupDialog.gameId = gameId
            largeBackupDialog.text = qsTr("Os saves de %1 têm %2, mais do que o perfil do jogo espera (%3). Isso pode indicar arquivos que não são saves. Fazer o backup mesmo assim?")
                                     .arg(name).arg(size).arg(expected)
            largeBackupDialog.open()
        }
    }

    // Confirms a backup larger than the game's profile expects (backup.maxBytes).
    Popup {
        id: largeBackupDialog
        objectName: "largeBackupDialog"
        property string gameId
        property string text
        modal: true
        focus: true
        width: 480
        anchors.centerIn: Overlay.overlay
        padding: Theme.space6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: Theme.radiusCard
            color: Theme.surface
            border.color: Theme.borderStrong
        }
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text {
                Layout.fillWidth: true
                text: qsTr("Backup maior que o esperado")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textSectionTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: largeBackupDialog.text
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                spacing: Theme.space3
                Item { Layout.fillWidth: true }
                SecondaryButton {
                    text: qsTr("Cancelar")
                    onClicked: largeBackupDialog.close()
                }
                PrimaryButton {
                    text: qsTr("Fazer backup")
                    onClicked: {
                        largeBackupDialog.close()
                        AppController.backupGameAnyway(largeBackupDialog.gameId)
                    }
                }
            }
        }
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

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: {
            sidebar.currentIndex = 0
            titleBar.focusSearch()
        }
    }
    Shortcut {
        sequence: "Ctrl+B"
        enabled: window.canActOnGame
        onActivated: libraryPage.backupSelected()
    }
    Shortcut {
        sequence: "Ctrl+R"
        enabled: window.canActOnGame
        onActivated: libraryPage.restoreSelected()
    }
    Shortcut {
        sequence: "Ctrl+T"
        enabled: window.canActOnGame
        onActivated: libraryPage.switchSaveSelected()
    }

    // The frame (title bar, rail, hint bar) uses Theme.window; the pages sit
    // on a raised panel with a rounded corner. The title bar fills the system
    // caption area, so the content below it starts after the safe area.
    Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(implicitHeight, window.contentItem.SafeArea.margins.top)
                hostWindow: window
                // Native buttons: traffic lights (about 70 px) on macOS,
                // three 46 px caption buttons on Windows.
                leftInset: window.integratedTitleBar && Qt.platform.os === "osx" ? 72 : 0
                rightInset: window.integratedTitleBar && Qt.platform.os === "windows" ? 140 : 0
                onSearchTextChanged: if (searchText.length > 0) sidebar.currentIndex = 0
                onSearchSubmitted: libraryPage.focusList()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Separates the title bar from the content panel, so the two
                // read as different layers.
                Layout.topMargin: 6
                spacing: 0

                Sidebar {
                    id: sidebar
                    objectName: "sidebar"
                    Layout.fillHeight: true
                    Layout.preferredWidth: 64
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.bg
                    topLeftRadius: Theme.radiusPanel
                    border.color: Theme.border

                    StackLayout {
                        anchors.fill: parent
                        anchors.topMargin: 1
                        anchors.leftMargin: 1
                        currentIndex: sidebar.currentIndex

                        LibraryPage {
                            id: libraryPage
                            objectName: "libraryPage"
                            selectedGameId: window.selectedGameId
                            search: titleBar.searchText
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
                    }
                }
            }

            HintBar {
                Layout.fillWidth: true
                hints: window.onLibrary
                       ? [{ key: "↑ ↓", label: qsTr("Navegar") },
                          { key: "Ctrl B", label: qsTr("Backup") },
                          { key: "Ctrl R", label: qsTr("Restaurar") },
                          { key: "Ctrl T", label: qsTr("Trocar save") },
                          { key: "Ctrl K", label: qsTr("Buscar") }]
                       : [{ key: "Ctrl K", label: qsTr("Buscar jogo") }]
                statusText: AppController.vaultReady
                            ? Format.count(AppController.library.count, qsTr("jogo"), qsTr("jogos")) + qsTr(" · vault em ") + AppController.vaultPath
                            : qsTr("Vault indisponível")
            }
        }
    }

    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space6
        anchors.bottomMargin: Theme.space6 + 34
    }
}
