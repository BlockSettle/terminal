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
    signal openExplorer(string txId)

    property int row
    property int column
    property var model

    Action {
        id: rbf_action
        text: qsTr("RBF")
        onTriggered: {
            var txId = model.data(model.index(context_menu.row, context_menu.column), TxListModel.TxIdRole)
            context_menu.openSend(txId, true, false)
        }
    }

    Action {
        id: cpfp_action
        text: qsTr("CPFP")
        onTriggered: {
            var txId = model.data(model.index(context_menu.row, context_menu.column), TxListModel.TxIdRole)
            context_menu.openSend(txId, false, true)
        }
    }

    Action {
        text: qsTr("View in explorer")
        onTriggered: {
            var txId = model.data(model.index(context_menu.row, context_menu.column), TxListModel.TxIdRole)
            context_menu.openExplorer(txId)
        }
    }

    onOpened: {
        rbf_action.enabled = (model.data(model.index(context_menu.row, context_menu.column), TxListModel.RBFRole)
            && model.data(model.index(context_menu.row, context_menu.column), TxListModel.NbConfRole) === 0
            && (model.data(model.index(context_menu.row, context_menu.column), TxListModel.DirectionRole) === 2 
                || model.data(model.index(context_menu.row, context_menu.column), TxListModel.DirectionRole) === 3))
        cpfp_action.enabled =  (model.data(model.index(context_menu.row, context_menu.column), TxListModel.NbConfRole) === 0
            && (model.data(model.index(context_menu.row, context_menu.column), TxListModel.DirectionRole) === 1 
                || model.data(model.index(context_menu.row, context_menu.column), TxListModel.DirectionRole) === 3))
    }
}
