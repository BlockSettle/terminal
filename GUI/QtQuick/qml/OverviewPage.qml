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
import "WalletProperties"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper
import wallet.balance 1.0

Rectangle {
    id: overview
    property int walletIndex: 0
    color: BSStyle.backgroundColor

    signal newWalletClicked();
    signal curWalletIndexChanged(index : int)
    signal openSend (string txId, bool isRBF, bool isCPFP)
    signal openExplorer (string txId)

    AddressDetails {
        id: addressDetails
        visible: false
    }

    WalletPropertiesPopup {
        id: walletProperties
        visible: false

        wallet_properties_vm: bsApp.walletProperitesVM
    }

    Overview.OverviewPanel {
        anchors.fill: parent

        onRequestWalletProperties: {
            bsApp.getUTXOsForWallet(walletIndex)
            walletProperties.show()
            walletProperties.raise()
            walletProperties.requestActivate()
        }
        onCreateNewWallet: overview.newWalletClicked()
        onWalletIndexChanged: {
            walletIndex = index
            overview.curWalletIndexChanged(index)
        }
        onOpenAddressDetails: (address, transactions, balance, comment, asset_type, type, wallet) => {
            addressDetails.address = address
            addressDetails.transactions = transactions
            addressDetails.balance = balance
            addressDetails.comment = comment
            addressDetails.asset_type = asset_type
            addressDetails.type = type
            addressDetails.wallet = wallet
            bsApp.startAddressSearch(address)
            addressDetails.open()
        }

        onOpenSend: (txId, isRBF, isCPFP) => overview.openSend(txId, isRBF, isCPFP)
        onOpenExplorer: (txId) => overview.openExplorer(txId)
    }
}
