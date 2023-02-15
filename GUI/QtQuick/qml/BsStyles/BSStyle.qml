/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
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


    readonly property color disabledTextColor: "#71787f"
    readonly property color disabledBgColor: "#31383f"

    readonly property color textColor: "white"
    readonly property color textPressedColor: "#3a8ab4"
    readonly property color disabledHeaderColor: "#909090"
    readonly property color titleTextColor: "#7A88B0"

    readonly property color labelsTextColor: "#939393"
    readonly property color labelsTextDisabledColor: "#454E53"
    readonly property color inputsBorderColor: "#757E83"
    readonly property color inputsFontColor: "white"
    readonly property color inputsInvalidColor: "red"
    readonly property color inputsValidColor: "green"
    readonly property color inputsPendingColor: "#f6a724"

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
    //readonly property color switchDisabledBgColor: disabledColor
    readonly property color switchDisabledColor: disabledTextColor

    readonly property color dialogHeaderColor: "#0A1619"
    readonly property color dialogTitleGreenColor: "#38C673"
    readonly property color dialogTitleOrangeColor: "#f7b03a"
    readonly property color dialogTitleRedColor: "#EE2249"
    readonly property color dialogTitleWhiteColor: "white"

    readonly property color comboBoxBgColor: "transparent"
    readonly property color comboBoxItemBgColor: "#17262b"
    readonly property color comboBoxItemBgHighlightedColor: "#27363b"


    readonly property color mainnetColor: "#fe9727"
    readonly property color testnetColor: "#22c064"
    readonly property color mainnetTextColor: "white"
    readonly property color testnetTextColor: "black"

    readonly property color selectedColor: "white"



    //new properties
    readonly property color defaultGreyColor: "#3C435A"

    readonly property color buttonsStandardColor: defaultGreyColor
    readonly property color buttonsStandardPressedColor: "#232734"
    readonly property color buttonsStandardHoveredColor: "#2E3343"
    readonly property color buttonsStandardBorderColor: "#FFFFFF"

    readonly property color buttonsPreferredColor: "#45A6FF"
    readonly property color buttonsPreferredPressedColor: "#0077E4"
    readonly property color buttonsPreferredHoveredColor: "#0085FF"
    readonly property color buttonsPreferredBorderColor: "#FFFFFF"

    readonly property color buttonsTextColor: "#FFFFFF"
    readonly property color buttonsHeaderTextColor: "#7A88B0"
    readonly property color buttonsDisabledTextColor: "#1C2130"

    readonly property color buttonsDisabledColor: "#32394F"

    readonly property color comboBoxItemTextColor: "#020817"
    readonly property color comboBoxItemTextHighlightedColor: "#45A6FF"
    readonly property color comboBoxItemTextCurrentColor: "#7A88B0"
    readonly property color comboBoxItemHighlightedColor: "#45A6FF"

    readonly property color comboBoxBorderColor: defaultGreyColor
    readonly property color comboBoxHoveredBorderColor: "#7A88B0"
    readonly property color comboBoxFocusedBorderColor: "#FFFFFF"
    readonly property color comboBoxPopupedBorderColor: "#45A6FF"

    readonly property color comboBoxIndicatorColor: "#DCE2FF"
    readonly property color comboBoxPopupedIndicatorColor: "#45A6FF"

    readonly property color tableSeparatorColor: defaultGreyColor
    readonly property color tableCellBackgroundColor: "transparent"
    readonly property color tableCellSelectedBackgroundColor: "#22293B"

    readonly property color balanceValueTextColor: "#E2E7FF"
    readonly property color addressesPanelBackgroundColor: "#333C435A"

    readonly property color listItemBorderColor: "#3C435A"

    readonly property color popupBackgroundColor: "#191E2A"
    readonly property color popupBorderColor: defaultGreyColor

    readonly property color transactionConfirmationZero: "#EB6060"
    readonly property color transactionConfirmationLow: "yellow"
    readonly property color transactionConfirmationHigh: "#67D2A3"

    readonly property color defaultBorderColor: defaultGreyColor

    //not colors
    readonly property int defaultPrecision: 8
}
