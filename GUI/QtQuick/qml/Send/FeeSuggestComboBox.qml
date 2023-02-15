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

CustomComboBox {

    id: fee_suggest_combo

    model: feeSuggestions

    //aliases
    title_text: qsTr("Fee Suggestions")

    height: 70

    textRole: "text"
    valueRole: "value"

    onActivated: (index_act) => {
        txInputsModel.fee = parseFloat(fee_suggest_combo.currentValue)
    }

    Connections
    {
        target:feeSuggestions
        function onRowCountChanged ()
        {
            fee_suggest_combo.currentIndex = 0
        }
    }
}
