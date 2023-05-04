/*

***********************************************************************************
* Copyright (C) 2018 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

import "../BsStyles"
import "../StyledControls"

CustomComboBox {
    id: fee_suggest_combo

    model: feeSuggestions
    editable: currentIndex == (feeSuggestions.rowCount - 1)

    //aliases
    title_text: qsTr("Fee Suggestions")

    height: BSSizes.applyScale(70)

    textRole: (currentIndex == (feeSuggestions.rowCount - 1) && !popup.visible) ? "value" : "text"
    valueRole: "value"
    suffix_text: qsTr("s/b")

    validator: RegExpValidator {regExp: new RegExp(create_regexp())}

    Connections
    {
        target:feeSuggestions
        function onRowCountChanged ()
        {
            fee_suggest_combo.currentIndex = 0
            if (typeof change_index_handler === "function")
            {
                change_index_handler()
            }
            if (typeof setup_fee === "function") {
                setup_fee()
            }

            validator.regExp = new RegExp(create_regexp())
        }
    }

    onCurrentIndexChanged: {
        if (typeof change_index_handler === "function")
        {
            change_index_handler()
        }
        validator.regExp = new RegExp(create_regexp())

        if (currentIndex == (feeSuggestions.rowCount - 1)) {
            fee_suggest_combo.input_item.forceActiveFocus()
            fee_suggest_combo.input_item.cursorPosition = 0
        }
    }

    function create_regexp()
    {
        return "\\d*\\.?\\d?"
    }

    function edit_value()
    {
        var res;
        if (currentIndex != (feeSuggestions.rowCount - 1)) {
            res = fee_suggest_combo.currentText
            var index = res.indexOf(":")
            res = res.slice(index+2)
            res = res.replace(" " + fee_suggest_combo.suffix_text, "")
        }
        else {
            res = fee_suggest_combo.input_text
        }
        return res
    }
}
