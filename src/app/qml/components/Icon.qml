import QtQuick
import QtQuick.Shapes

// Ícone de traço desenhado a partir de um caminho SVG numa grade 24x24.
// Os caminhos ficam no singleton Icons.
Item {
    id: root

    property string path
    property color color: "white"
    property real strokeWidth: 2

    implicitWidth: 18
    implicitHeight: 18

    Shape {
        width: 24
        height: 24
        scale: Math.min(root.width, root.height) / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.path }
        }
    }
}
