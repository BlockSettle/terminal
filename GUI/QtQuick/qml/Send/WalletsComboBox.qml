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

import wallet.balance 1.0

CustomComboBox {

    id: from_wallet_combo

    height: BSSizes.applyScale(70)

    model: walletBalances
    currentIndex: walletBalances.selectedWallet

    //aliases
    title_text: qsTr("From Wallet")
    details_text: walletBalances.totalBalance

    textRole: "name"
    valueRole: "name"
}
