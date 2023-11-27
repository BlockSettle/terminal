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

    id: rec_addr_input

    property string fee_current_value
    property int wallets_current_index

    signal focus_next()

    input_right_margin: BSSizes.applyScale(paste_but.anchors.rightMargin + paste_but.width + 16)

    //aliases
    title_text: qsTr("Receiver address")

    Image {
        id: paste_but

        z: 1

        anchors.top: rec_addr_input.top
        anchors.topMargin: BSSizes.applyScale(23)
        anchors.right: rec_addr_input.right
        anchors.rightMargin: BSSizes.applyScale(23)

        source: "qrc:/images/paste_icon.png"
        width: BSSizes.applyScale(24)
        height: BSSizes.applyScale(24)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                rec_addr_input.input_text = bsApp.pasteTextFromClipboard()
                rec_addr_input.validate()
            }
        }
    }

    onTextEdited : {
        rec_addr_input.validate()
    }

    function validate()
    {
        if (rec_addr_input.input_text.length)
        {
            rec_addr_input.isValid = bsApp.validateAddress(rec_addr_input.input_text)
            if (rec_addr_input.isValid)
            {
                create_temp_request()
                focus_next()
            }
        }
        else
        {
            rec_addr_input.isValid = true
        }
    }
}
