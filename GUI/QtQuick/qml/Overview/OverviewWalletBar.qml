/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import wallet.balance 1.0

import "../BsStyles"
import "." as OverviewControls
import "../StyledControls" as Controls

Rectangle {
    id: control
    color: "transparent"

    property alias currentWallet: wallet_selection_combobox.currentValue

    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)

    Row {
        anchors.fill:parent
        spacing: BSSizes.applyScale(20)

        Controls.CustomComboBox {
            id: wallet_selection_combobox
            anchors.verticalCenter: parent.verticalCenter
            objectName: "walletsComboBox"

            width: BSSizes.applyScale(263)
            height: BSSizes.applyScale(53)

            model: walletBalances.rowCount > 0 ? walletBalances : [{ "name": qsTr("Create wallet")}]
            textRole: "name"
            valueRole: "name"

            fontSize: BSSizes.applyScale(16)

            onCurrentIndexChanged: {
                if (walletBalances.rowCount !== 0) {
                    bsApp.walletSelected(wallet_selection_combobox.currentIndex)
                    control.walletIndexChanged(wallet_selection_combobox.currentIndex)
                    walletBalances.selectedWallet = wallet_selection_combobox.currentIndex
                }
            }

            onActivated: {
                if (walletBalances.rowCount === 0) {
                    control.createNewWallet()
                }
                else {
                    bsApp.walletSelected(wallet_selection_combobox.currentIndex)
                    control.walletIndexChanged(wallet_selection_combobox.currentIndex)
                    walletBalances.selectedWallet = wallet_selection_combobox.currentIndex
                }
            }

            onModelChanged: {
                if (model === walletBalances) {
                    wallet_selection_combobox.currentIndex = walletBalances.selectedWallet
                }
            }

            Connections {
                target: bsApp
                function onRequestWalletSelection(index) {
                    bsApp.walletSelected(index)
                    control.walletIndexChanged(index)
                    wallet_selection_combobox.currentIndex = index
                }
            }
        }

        OverviewControls.BalanceBar {
            id: balance_bar
            anchors.verticalCenter: parent.verticalCenter

            confirmed_balance_value: walletBalances.confirmedBalance
            uncorfirmed_balance_value: walletBalances.unconfirmedBalance
            total_balance_value: walletBalances.totalBalance
            used_addresses_value: walletBalances.numberAddresses
        }
    }

    Row {
        spacing: BSSizes.applyScale(10)
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right

        Controls.CustomMediumButton {
            text: qsTr("Wallet Properties")
            onClicked: control.requestWalletProperties()
        }

        Controls.CustomMediumButton {
            text: qsTr("Create new wallet")
            onClicked: control.createNewWallet()
        }
    }
}
