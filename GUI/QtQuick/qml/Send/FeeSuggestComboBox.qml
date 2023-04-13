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
    editable: true

    //aliases
    title_text: qsTr("Fee Suggestions")

    height: BSSizes.applyScale(70)

    textRole: "text"
    valueRole: "value"

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

            validator.regExp = new RegExp(create_regexp())
        }
    }

    onCurrentIndexChanged: {
        if (typeof change_index_handler === "function")
        {
            change_index_handler()
        }
        validator.regExp = new RegExp(create_regexp())
    }

    function create_regexp()
    {
        var res = fee_suggest_combo.currentText
        var index = res.indexOf(":")
        res = res.slice(0, index+2)
        res = res.replace("(", "\\(").replace(")", "\\)")
        res = res + "\\d*\\.?\\d? s\/b"
        return res
    }

    function edit_value()
    {
        var res = fee_suggest_combo.input_text
        var index = res.indexOf(":")
        res = res.slice(index+2)
        res = res.replace(" s/b", "")
        return res
    }

    // property string prev_text : fee_suggest_combo.currentText
    // onTextEdited : {
    //     if (!fee_suggest_combo.input_accept_input)
    //     {
    //         fee_suggest_combo.input_text = prev_text
    //     }
    //     prev_text = fee_suggest_combo.input_text
    // }

    onEditingFinished : {
        if (!edit_value())
        {
            fee_suggest_combo.input_text = fee_suggest_combo.currentText
        }
    }
}
