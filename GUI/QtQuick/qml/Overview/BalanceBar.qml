/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

import "../BsStyles"
import "." as OverviewControls

Rectangle {
    id: control

    width: BSSizes.applyScale(520)
    height: BSSizes.applyScale(100)
    color: "transparent"

    property alias confirmed_balance_value: confirmed_balance.label_value
    property alias uncorfirmed_balance_value: unconfirmed_balance.label_value
    property alias total_balance_value: total_balance.label_value
    property alias used_addresses_value: used_addresses.label_value

    property int spacer_height: BSSizes.applyScale(36)

    Row {
        anchors.fill: parent
        spacing: BSSizes.applyScale(10)

        OverviewControls.BaseBalanceLabel {
            id: confirmed_balance
            width: BSSizes.applyScale(130)
            label_text: qsTr("Confirmed balance")
            value_suffix: qsTr('BTC')
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: BSStyle.tableSeparatorColor
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: unconfirmed_balance
            width: BSSizes.applyScale(130)
            label_text: qsTr("Unconfirmed balance")
            value_suffix: qsTr('BTC')
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: BSStyle.tableSeparatorColor
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: total_balance
            width: BSSizes.applyScale(130)
            label_text: qsTr("Total balance")
            value_suffix: qsTr('BTC')
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: BSStyle.tableSeparatorColor
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: used_addresses
            label_text: qsTr("#Used addresses")
            value_suffix: ""
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
