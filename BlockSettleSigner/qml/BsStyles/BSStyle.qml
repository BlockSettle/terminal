/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
pragma Singleton
import QtQuick 2.0

//color names taken from http://chir.ag/projects/name-that-color

QtObject {
    readonly property color backgroundColor: "#1c2835"
    readonly property color backgroundPressedColor: "#2c3845"
    readonly property color backgroundModalColor: "#737373"
    readonly property color backgroundModeLessColor: "#939393"

    readonly property color disabledColor: "#41484f"
    readonly property color disabledTextColor: "#71787f"
    readonly property color disabledBgColor: "#31383f"

    readonly property color textColor: "white"
    readonly property color textPressedColor: "#3a8ab4"
    readonly property color disabledHeaderColor: "#909090"

    readonly property color labelsTextColor: "#939393"
    readonly property color labelsTextDisabledColor: "#454E53"
    readonly property color inputsBorderColor: "#757E83"
    readonly property color inputsFontColor: "white"
    readonly property color inputsInvalidColor: "red"
    readonly property color inputsValidColor: "green"

    readonly property color buttonsMainColor: "transparent"
    readonly property color buttonsPressedColor: "#55000000"
    readonly property color buttonsHoveredColor: "#22000000"

    readonly property color buttonsPrimaryMainColor: "#247dac"
    readonly property color buttonsPrimaryPressedColor: "#22C064"
    readonly property color buttonsPrimaryHoveredColor: "#449dcc"

    readonly property color buttonsUncheckedColor: "#81888f"
    readonly property color buttonsBorderColor: "#247dac"

    readonly property color progressBarColor: "#22C064"
    readonly property color progressBarBgColor: "black"

    readonly property color switchBgColor: "transparent"
    //readonly property color switchCheckedColor: "#22C064"
    readonly property color switchCheckedColor: "#247dac"
    readonly property color switchOrangeColor: "#f6a724"
    readonly property color switchUncheckedColor: "#b1b8bf"
    readonly property color switchDisabledBgColor: disabledColor
    readonly property color switchDisabledColor: disabledTextColor

    readonly property color dialogHeaderColor: "#0A1619"
    readonly property color dialogTitleGreenColor: "#38C673"
    readonly property color dialogTitleOrangeColor: "#f7b03a"
    readonly property color dialogTitleRedColor: "#EE2249"
    readonly property color dialogTitleWhiteColor: "white"

    readonly property color comboBoxBgColor: "transparent"
    readonly property color comboBoxItemBgColor: "#17262b"
    readonly property color comboBoxItemBgHighlightedColor: "#27363b"
    readonly property color comboBoxItemTextColor: textColor
    readonly property color comboBoxItemTextHighlightedColor: textColor
}
