pragma Singleton
import QtQuick

// Text helpers for the interface (pt-BR).
QtObject {
    // "22.262"
    function number(n) {
        return Number(n).toLocaleString(Qt.locale("pt_BR"), "f", 0)
    }

    // "1 jogo", "30 jogos"
    function count(n, singular, plural) {
        return number(n) + " " + (n === 1 ? singular : plural)
    }
}
