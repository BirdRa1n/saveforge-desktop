import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// One game in the library list: cover thumbnail, name, store and backup status.
// `selected` draws the accent ring the keyboard focus follows.
AbstractButton {
    id: control

    property string name
    property string store
    property int snapshotCount
    property string statusText
    property int statusKind: StatusBadge.Neutral
    property url coverSource
    property bool iconOnly: false
    property color placeholderColor: Theme.neutralBg
    property bool selected: false

    implicitHeight: 52
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    Accessible.role: Accessible.ListItem
    Accessible.name: name + ", " + store + ", " + statusText
    Accessible.selected: selected

    function initials(value) {
        return value.split(/\s+/).filter(w => w.length > 0).map(w => w[0]).join("").slice(0, 2).toUpperCase()
    }

    background: Rectangle {
        radius: Theme.radiusControl + 2
        color: control.selected ? Theme.accentTint
             : control.hovered ? Theme.surfaceRaised : "transparent"
        border.width: control.selected ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: RowLayout {
        spacing: Theme.space3

        // Thumbnail: the cover cropped square, the store icon, or initials.
        Rectangle {
            Layout.leftMargin: Theme.space2
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: Theme.radiusControl
            color: control.placeholderColor

            Text {
                anchors.centerIn: parent
                visible: control.iconOnly ? icon.status !== Image.Ready : cover.status !== Image.Ready
                text: control.initials(control.name)
                color: Qt.rgba(1, 1, 1, 0.9)
                font.family: Theme.fontDisplay
                font.pixelSize: 12
                font.weight: Font.Bold
            }
            RoundedImage {
                id: cover
                anchors.fill: parent
                visible: !control.iconOnly
                radius: Theme.radiusControl
                source: control.iconOnly ? "" : control.coverSource
            }
            Image {
                id: icon
                anchors.centerIn: parent
                width: 24
                height: 24
                visible: control.iconOnly
                source: control.iconOnly ? control.coverSource : ""
                sourceSize: Qt.size(48, 48)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: control.name
                color: Theme.text
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: control.store + "  ·  " + Format.count(control.snapshotCount, qsTr("backup"), qsTr("backups"))
                color: Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: Theme.textMeta
                elide: Text.ElideRight
            }
        }

        // Status: a dot in the status colour plus its text (never colour alone).
        RowLayout {
            Layout.preferredWidth: 132
            Layout.rightMargin: Theme.space2
            spacing: 6
            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: label.color
            }
            Text {
                id: label
                Layout.fillWidth: true
                text: control.statusText
                color: control.statusKind === StatusBadge.Success ? Theme.successFg
                     : control.statusKind === StatusBadge.Warning ? Theme.warningFg : Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }
    }
}
