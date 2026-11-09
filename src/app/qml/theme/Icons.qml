pragma Singleton
import QtQuick

// Caminhos SVG (grade 24x24, traço) dos ícones da interface.
QtObject {
    readonly property string anvil: "M4 15h16l-2 5H6z M8 15V9h8v6 M12 9V4 M9 6l3-2 3 2"
    readonly property string library: "M3 3h7v7H3z M14 3h7v7h-7z M3 14h7v7H3z M14 14h7v7h-7z"
    readonly property string activity: "M22 12h-4l-3 9L9 3l-3 9H2"
    readonly property string transfer: "M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4 M7 10l5 5 5-5 M12 15V3"
    readonly property string settings: "M12 9a3 3 0 1 0 0.01 0z M12 2v3 M12 19v3 M2 12h3 M19 12h3 M4.9 4.9l2.1 2.1 M17 17l2.1 2.1 M4.9 19.1l2.1-2.1 M17 7l2.1-2.1"
    readonly property string search: "M11 4a7 7 0 1 0 0.01 0z M21 21l-4.3-4.3"
    readonly property string back: "M15 18l-6-6 6-6"
    readonly property string refresh: "M21 12a9 9 0 1 1-2.6-6.4 M21 4v5h-5"
    readonly property string backup: "M12 3v12 M7 10l5 5 5-5 M5 21h14"
    readonly property string restore: "M3 12a9 9 0 1 0 2.6-6.4 M3 4v5h5"
    readonly property string warning: "M12 9v4 M12 17h.01 M10.3 3.9L1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z"
    readonly property string check: "M20 6L9 17l-5-5"
    readonly property string folder: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
    readonly property string loader: "M12 3a9 9 0 1 0 9 9"
    readonly property string gamepad: "M6 11h4 M8 9v4 M15 12h.01 M18 10h.01 M17.3 5H6.7a4 4 0 0 0-3.96 3.43L2 14.5a2.5 2.5 0 0 0 4.4 1.9L8 14.5h8l1.6 1.9a2.5 2.5 0 0 0 4.4-1.9l-.74-6.07A4 4 0 0 0 17.3 5z"
}
