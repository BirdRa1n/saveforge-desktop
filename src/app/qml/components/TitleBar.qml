import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveVault.Backend

// Unified title bar: on Windows and macOS it is drawn in the system caption
// area, next to the native window buttons, with the app mark, the "command
// center" search (Ctrl+K) and background progress. Dragging the empty parts
// moves the window and a double click maximizes or restores it, like a
// native title bar. On Linux it is an ordinary toolbar under the system frame.
Item {
    id: root

    // The window this bar moves; set by Main.qml.
    required property Window hostWindow
    // Room kept free for the native window buttons (traffic lights on the
    // left in macOS, minimize/maximize/close on the right in Windows).
    property real leftInset: 0
    property real rightInset: 0
    property alias searchText: search.text

    // Enter or Down in the search: move to the results.
    signal searchSubmitted()

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    // Matches the height of the native window buttons, so their glyphs and
    // the search line up.
    implicitHeight: 32

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
        anchors.left: parent.left
        anchors.leftMargin: root.leftInset + Theme.space4
        anchors.verticalCenter: parent.verticalCenter
    }

    SearchInput {
        id: search
        objectName: "commandBar"
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 2 * Math.max(root.leftInset, root.rightInset) - 360)
        // A light control in a 32 px bar: 4 px of air above and below.
        implicitHeight: 24
        compact: true
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
        anchors.rightMargin: root.rightInset + Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2
        visible: AppController.busy

        Spinner {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            running: parent.visible
        }
        Text {
            Layout.maximumWidth: 180
            text: AppController.busyText
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textMeta
            elide: Text.ElideRight
        }
    }
}
