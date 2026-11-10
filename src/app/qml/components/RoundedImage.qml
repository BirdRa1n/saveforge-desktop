import QtQuick
import QtQuick.Effects

// Image clipped to rounded corners (Item.clip only clips to rectangles).
// Each corner can have its own radius, e.g. only the top ones on a card.
Item {
    id: root

    property alias source: image.source
    property alias fillMode: image.fillMode
    property alias status: image.status
    property real radius: Theme.radiusCard
    property real topLeftRadius: radius
    property real topRightRadius: radius
    property real bottomLeftRadius: radius
    property real bottomRightRadius: radius

    Image {
        id: image
        anchors.fill: parent
        visible: false
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.ceil(root.width * Screen.devicePixelRatio)
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        topLeftRadius: root.topLeftRadius
        topRightRadius: root.topRightRadius
        bottomLeftRadius: root.bottomLeftRadius
        bottomRightRadius: root.bottomRightRadius
    }

    MultiEffect {
        anchors.fill: parent
        visible: image.status === Image.Ready
        source: image
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
