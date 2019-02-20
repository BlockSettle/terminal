pragma Singleton
import QtQuick 2.0

//color names taken from http://chir.ag/projects/name-that-color

QtObject {
    property color backgroundColor: "#1c2835"
    property color backgroundPressedColor: "#2c3845"
    property color backgroundModalColor: "#737373"
    property color backgroundModeLessColor: "#939393"

    property color textColor: "white"
    property color textPressedColor: "#3a8ab4"

    property color labelsTextColor: "#757E83"
    property color labelsTextDisabledColor: "#454E53"
    property color inputsBorderColor: "#757E83"
    property color inputsFontColor: "white"
    property color inputsInvalidColor: "red"
    property color inputsValidColor: "green"

    property color buttonsMainColor: "transparent"
    property color buttonsPressedColor: "#55000000"
    property color buttonsHoveredColor: "#22000000"

    property color buttonsPrimaryMainColor: "#247dac"
    property color buttonsPrimaryPressedColor: "#22C064"
    property color buttonsPrimaryHoveredColor: "#449dcc"

    property color buttonsUncheckedColor: "#81888f"
    property color buttonsBorderColor: "#247dac"

    property color progressBarColor: "#22C064"
    property color progressBarBgColor: "black"

    property color switchBgColor: "transparent"
    //property color switchCheckedColor: "#22C064"
    property color switchCheckedColor: "#247dac"
    property color switchOrangeColor: "#f6a724"
    //property color switchUncheckedColor: "#EC0A35"
    property color switchUncheckedColor: "#81888f"

    property color dialogHeaderColor: "#0A1619"
    property color dialogTitleGreenColor: "#38C673"
    property color dialogTitleOrangeColor: "#f7b03a"
    property color dialogTitleRedColor: "#EE2249"
    property color dialogTitleWhiteColor: "white"

    property color comboBoxBgColor: "transparent"
    property color comboBoxItemBgColor: "#17262b"
    property color comboBoxItemBgHighlightedColor: buttonsMainColor
    property color comboBoxItemTextColor: textColor
    property color comboBoxItemTextHighlightedColor: textColor

}
