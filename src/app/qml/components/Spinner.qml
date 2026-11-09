import QtQuick

// Indeterminate progress indicator in the accent color.
Icon {
    id: root

    property bool running: true

    path: Icons.loader
    color: Theme.accent
    visible: running
    Accessible.role: Accessible.Animation
    Accessible.name: qsTr("Carregando")

    RotationAnimator on rotation {
        running: root.running && root.visible
        from: 0
        to: 360
        duration: 900
        loops: Animation.Infinite
    }
}
