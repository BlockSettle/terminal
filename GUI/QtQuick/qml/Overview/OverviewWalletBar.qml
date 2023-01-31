/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import wallet.balance 1.0

import "." as OverviewControls

Rectangle {
    id: control

    width: 1200
    height: 100
    color: "#191E2A"

    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)

    Row {
        anchors.fill:parent
        spacing: 20

        OverviewControls.BaseCombobox {
            id: wallet_selection_combobox
            anchors.verticalCenter: parent.verticalCenter

            objectName: "walletsComboBox"
            model: walletBalances
            textRole: "name"
            valueRole: "name"

            Component.onCompleted: {
                control.walletIndexChanged(0)
            }

            onActivated: (ind) => {
                             control.walletIndexChanged(ind)
                         }
        }

        OverviewControls.BalanceBar {
            id: balance_bar
            anchors.verticalCenter: parent.verticalCenter

            confirmed_balance_value: walletBalances.data(walletBalances.index(wallet_selection_combobox.currentIndex, 0),
                                                         WalletBalance.ConfirmedRole).toFixed(5)
            uncorfirmed_balance_value: walletBalances.data(walletBalances.index(wallet_selection_combobox.currentIndex, 0),
                                                           WalletBalance.UnconfirmedRole).toFixed(5)
            total_balance_value: walletBalances.data(walletBalances.index(wallet_selection_combobox.currentIndex, 0),
                                                     WalletBalance.TotalRole).toFixed(5)
            used_addresses_value: walletBalances.data(walletBalances.index(wallet_selection_combobox.currentIndex, 0),
                                                WalletBalance.NbAddrRole)
        }
    }

    Row {
        spacing: 10
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right

        OverviewControls.BaseWalletButton {
            text: "Wallet properties"
            onClicked: control.requestWalletProperties()
        }

        OverviewControls.BaseWalletButton {
            text: "Create new wallet"
            onClicked: control.createNewWallet()
        }
    }
}
