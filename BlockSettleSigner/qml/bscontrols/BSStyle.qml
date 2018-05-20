pragma Singleton
import QtQuick 2.0

//color names taken from http://chir.ag/projects/name-that-color

QtObject {
    property color greyRollingStone: "#757E83"

    property color inputsBorderColor: greyRollingStone
    property color inputsFontColor: "white"
    property color inputsInvalidColor: "red"
    property color inputsValidColor: "green"

    property color labelsFontColor: greyRollingStone
}
