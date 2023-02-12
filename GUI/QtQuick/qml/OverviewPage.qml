/*

***********************************************************************************
* Copyright (C) 2018 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2

import "Overview" as Overview
import "StyledControls"
import "BsStyles"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper
import wallet.balance 1.0

Item {
    id: overview

    signal newWalletClicked();
    signal curWalletIndexChanged(index : int)

    CreateNewWallet {
        id: createNewWalletPage
        visible: false
    }

    AddressDetails {
        id: addressDetails
        visible: false
    }

    Overview.OverviewPanel {
        anchors.fill: parent

        onRequestWalletProperties: console.log("Nothing to do")
        onCreateNewWallet: overview.newWalletClicked()
        onWalletIndexChanged: overview.curWalletIndexChanged(index)
        onOpenAddressDetails: (address, transactions, balance, comment, type, wallet) => {
            addressDetails.address = address
            addressDetails.transactions = transactions
            addressDetails.balance = balance
            addressDetails.comment = comment
            addressDetails.type = type
            addressDetails.wallet = wallet
            bsApp.startAddressSearch(address)
            addressDetails.open()
        }
    }
}
