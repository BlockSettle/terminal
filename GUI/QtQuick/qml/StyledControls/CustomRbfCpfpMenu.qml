/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

import QtQuick 2.9
import QtQuick.Controls 2.3

import "../BsStyles"

import terminal.models 1.0

CustomContextMenu {
    id: context_menu

    signal openSend (string txId, bool isRBF, bool isCPFP)

    property int row
    property int column
    property var model

    Action {
        text: qsTr("RBF")
        enabled: model.data(model.index(context_menu.row, context_menu.column), TxListModel.RBFRole)
            //&& (model.data(model.index(context_menu.row, context_menu.column), TxListModel.NbConfRole) === 0)
        onTriggered: {
            var txId = model.data(model.index(context_menu.row, context_menu.column), TxListModel.TxIdRole)
            context_menu.openSend(txId, true, false)
        }
    }

    Action {
        text: qsTr("CPFP")
        enabled: true//(model.data(model.index(context_menu.row, context_menu.column), TxListModel.NbConfRole) === 0)
        onTriggered: {
            var txId = model.data(model.index(context_menu.row, context_menu.column), TxListModel.TxIdRole)
            context_menu.openSend(txId, false, true)
        }
    }

}
