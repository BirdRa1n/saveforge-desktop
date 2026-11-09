import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Card de jogo da biblioteca. Sem capa, mostra um bloco de cor com as iniciais.
AbstractButton {
    id: control

    property string name
    property string store
    property string statusText
    property int statusKind: StatusBadge.Neutral
    property url coverSource
    property color placeholderColor: "#3B4A6B"

    implicitWidth: 180
    implicitHeight: 236
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: name + ", " + store + ", " + statusText

    function initials(value) {
        return value.split(/\s+/).filter(w => w.length > 0).map(w => w[0]).join("").slice(0, 2).toUpperCase()
    }

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.card
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent
                    : control.hovered ? Theme.borderStrong : Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            Layout.margins: 1
            topLeftRadius: Theme.radiusCard - 1
            topRightRadius: Theme.radiusCard - 1
            color: control.placeholderColor
            clip: true

            Text {
                anchors.centerIn: parent
                visible: cover.status !== Image.Ready
                text: control.initials(control.name)
                color: Qt.rgba(1, 1, 1, 0.88)
                font.family: Theme.fontDisplay
                font.pixelSize: 34
                font.weight: Font.Bold
            }

            Image {
                id: cover
                anchors.fill: parent
                source: control.coverSource
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space3 + 2
            Layout.topMargin: Theme.space3
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: control.name
                color: Theme.text
                font.family: Theme.fontBody
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                text: control.store
                color: Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: Theme.textMeta
            }
            StatusBadge {
                text: control.statusText
                kind: control.statusKind
            }
        }
    }
}
