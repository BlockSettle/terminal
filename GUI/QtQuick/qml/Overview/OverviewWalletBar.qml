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

import "." as OverviewControls
import "../StyledControls" as Controls

Rectangle {
    id: control

    width: 1200
    height: 100
    color: "transparent"

    property alias currentWallet: wallet_selection_combobox.currentValue

    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)

    Row {
        anchors.fill:parent
        spacing: 20

        Controls.CustomComboBox {
            id: wallet_selection_combobox
            anchors.verticalCenter: parent.verticalCenter
            objectName: "walletsComboBox"

            width: 263
            height: 53

            model: walletBalances
            textRole: "name"
            valueRole: "name"

            fontSize: 16

            onCurrentIndexChanged: {
                bsApp.walletSelected(currentIndex)
                control.walletIndexChanged(currentIndex)
                walletBalances.selectedWallet = currentIndex
            }

            Connections {
                target: bsApp
                function onRequestWalletSelection(index) {
                    if (wallet_selection_combobox.currentIndex != index) {
                        wallet_selection_combobox.currentIndex = index
                    }
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
        spacing: 10
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
