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

    //aliases
    title_text: qsTr("Amount")

    //app (if was launched from visual studio) crashes when there is input_validator
    //and we change text inside of onTextEdited
    //it is why I have realized my validator inside of onTextEdited
    property string prev_text : ""
    onTextEdited : {

        if (tempRequest == null) {
            amount_input.input_text = "0"
        }

        amount_input.input_text = amount_input.input_text.replace(",", ".")

        if (amount_input.input_text.startsWith("0")
            && !amount_input.input_text.startsWith("0.")
            && amount_input.input_text.length > 1)
        {
            amount_input.input_text = "0."
                    + amount_input.input_text.substring(1, amount_input.input_text.length)
        }
        try {
            var input_number =  Number.fromLocaleString(Qt.locale("en_US"), amount_input.input_text)
        }
        catch (error)
        {
            amount_input.input_text = prev_text
            return
        }

        if (input_number < 0 || ((tempRequest != null) && (input_number > tempRequest.maxAmount)))
        {
            amount_input.input_text = prev_text
            return
        }

        prev_text = amount_input.input_text
    }

    CustomButton {

        id: max_but

        z: 1

        width: 55
        height: 28
        back_radius: 37

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 23

        text: qsTr("MAX")
        font.pixelSize: 12
        enabled: (tempRequest != null)

        function click_enter() {
            if (tempRequest != null) {
                amount_input.input_text = tempRequest.maxAmount
            }
        }
    }

    Label {

        id: currency

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: max_but.left
        anchors.rightMargin: 16

        text: "BTC"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
        color: "#7A88B0"
    }

}
