/*

***********************************************************************************
* Copyright (C) 2018 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

import "../BsStyles"
import "../StyledControls"

CustomTextInput {

    id: amount_input

    property var balanceSubtractor: null

    //aliases
    title_text: qsTr("Amount")

    //app (if was launched from visual studio) crashes when there is input_validator
    //and we change text inside of onTextEdited
    //it is why I have realized my validator inside of onTextEdited
    property string prev_text : ""
    onTextEdited : {

       if (tempRequest === null || !tempRequest.isValid) {
           amount_input.input_text = "0"
       }

        amount_input.input_text = amount_input.input_text.replace(",", ".")

        var indexOfDot = amount_input.input_text.indexOf(".")
        if (indexOfDot >= 0)
        {
            amount_input.input_text = amount_input.input_text.substring(0,
                                      Math.min(indexOfDot+9, amount_input.input_text.length))
        }
        if (amount_input.input_text.startsWith("0")
            && !amount_input.input_text.startsWith("0.")
            && amount_input.input_text.length > 1)
        {
            amount_input.input_text = "0."
                    + amount_input.input_text.substring(1, amount_input.input_text.length)
        }
        try {
            var input_number = Number.fromLocaleString(Qt.locale("en_US"), amount_input.input_text)
        }
        catch (error)
        {
            amount_input.input_text = prev_text
            return
        }

        if (input_number < 0 || (input_number > tempRequest.maxAmount))
        {
            amount_input.input_text = prev_text
            return
        }

        prev_text = amount_input.input_text
    }

    CustomButton {

        id: max_but

        z: 1

        width: BSSizes.applyScale(55)
        height: BSSizes.applyScale(28)
        back_radius: BSSizes.applyScale(37)

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: BSSizes.applyScale(23)

        text: qsTr("MAX")
        font.pixelSize: BSSizes.applyScale(12)
        enabled: (tempRequest != null)

        function click_enter() {
            console.log(tempRequest, tempRequest.maxAmount)
            if (tempRequest != null && tempRequest.maxAmount.length > 0) {
                amount_input.input_text = (parseFloat(tempRequest.maxAmount) - (balanceSubtractor !== null ? balanceSubtractor : 0.0)).toFixed(8)
            }
        }
    }

    Label {

        id: currency

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: max_but.left
        anchors.rightMargin: BSSizes.applyScale(16)

        text: "BTC"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
        color: "#7A88B0"
    }

}
