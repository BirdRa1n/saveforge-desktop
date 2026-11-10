import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveVault.Backend

// Top bar of the window, right under the system caption area: the app mark,
// the search ("command bar", focused with Ctrl+K) and background progress.
// Dragging the empty parts moves the window, like a native title bar, and a
// double click maximizes or restores it.
Item {
    id: root

    // The window this bar moves; set by Main.qml.
    required property Window hostWindow
    // Hides the app mark where the system already draws icon and title (Windows).
    property bool showLogo: true
    property alias searchText: search.text

    // Enter or Down in the search: move to the results.
    signal searchSubmitted()

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    implicitHeight: 48

    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: root.hostWindow.visibility === Window.Maximized ? root.hostWindow.showNormal()
                                                                   : root.hostWindow.showMaximized()
    }
    DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        onActiveChanged: if (active) root.hostWindow.startSystemMove()
    }

    Logo {
        visible: root.showLogo
        anchors.left: parent.left
        anchors.leftMargin: Theme.space5
        anchors.verticalCenter: parent.verticalCenter
    }

    SearchInput {
        id: search
        objectName: "commandBar"
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 420)
        implicitHeight: 34
        placeholderText: qsTr("Buscar jogo…")
        Accessible.name: qsTr("Buscar jogo")
        shortcutHint: "Ctrl K"
        Keys.onReturnPressed: root.searchSubmitted()
        Keys.onDownPressed: root.searchSubmitted()
        Keys.onEscapePressed: {
            text = ""
            focus = false
        }
    }

    RowLayout {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space5
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2
        visible: AppController.busy

        Spinner {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            running: parent.visible
        }
        Text {
            Layout.maximumWidth: 200
            text: AppController.busyText
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textMeta
            elide: Text.ElideRight
        }
    }
}
