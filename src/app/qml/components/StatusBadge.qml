import QtQuick

// Badge de status de backup. O rótulo em texto é obrigatório:
// a cor nunca é a única forma de transmitir o estado.
Rectangle {
    id: root

    enum Kind { Success, Warning, Neutral }

    property int kind: StatusBadge.Neutral
    property alias text: label.text

    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 6
    radius: Theme.radiusBadge
    color: kind === StatusBadge.Success ? Theme.successBg
         : kind === StatusBadge.Warning ? Theme.warningBg : Theme.neutralBg

    Text {
        id: label
        anchors.centerIn: parent
        color: root.kind === StatusBadge.Success ? Theme.successFg
             : root.kind === StatusBadge.Warning ? Theme.warningFg : Theme.neutralFg
        font.family: Theme.fontBody
        font.pixelSize: Theme.textMeta
        font.weight: Font.DemiBold
    }
}
