
/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"
import ".."

import terminal.models 1.0

CustomTableView {
    id: control

    signal openSend (string txId, bool isRBF, bool isCPFP)
    signal openExplorer (string txId)

    copy_button_column_index: 3
    columnWidths: [0.12, 0.1, 0.08, 0.3, 0.1, 0.1, 0.1, 0.1]
    onCopyRequested: bsApp.copyAddressToClipboard(id)

    CustomRbfCpfpMenu {
        id: context_menu

        model: control.model

        onOpenSend: (txId, isRBF, isCPFP) => control.openSend(txId, isRBF, isCPFP)
        onOpenExplorer: (txId) => control.openExplorer(txId)
    }

    onCellClicked: (row, column, data, mouse) => {
        if (mouse.button === Qt.RightButton)
        {
            context_menu.row = row
            context_menu.column = column
            context_menu.popup()
        }
        else
        {
            const txHash = model.data(model.index(row, 0), TxListModel.TxIdRole)
            transactionDetails.walletName = model.data(model.index(row, 1), TxListModel.TableDataRole)
            transactionDetails.address = model.data(model.index(row, 3), TxListModel.TableDataRole)
            transactionDetails.txDateTime = model.data(model.index(row, 0), TxListModel.TableDataRole)
            transactionDetails.txType = model.data(model.index(row, 2), TxListModel.TableDataRole)
            transactionDetails.txTypeColor = model.data(model.index(row, 2), TxListModel.ColorRole)
            transactionDetails.txComment = model.data(model.index(row, 7), TxListModel.TableDataRole)
            transactionDetails.txAmount = model.data(model.index(row, 4), TxListModel.TableDataRole)
            transactionDetails.txConfirmationsColor = model.data(model.index(row, 5), TxListModel.ColorRole)
            transactionDetails.tx = bsApp.getTXDetails(txHash)
            transactionDetails.open()
        }
    }

    TransactionDetails {
        id: transactionDetails
        visible: false
    }
}
