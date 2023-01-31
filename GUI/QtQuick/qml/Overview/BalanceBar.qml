/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

import "." as OverviewControls

Rectangle {
    id: control

    width: 520
    height: 100
    color: "#191E2A"

    property alias confirmed_balance_value: confirmed_balance.label_value
    property alias uncorfirmed_balance_value: unconfirmed_balance.label_value
    property alias total_balance_value: total_balance.label_value
    property alias used_addresses_value: used_addresses.label_value

    property int spacer_height: 36
    property color spacer_color: "#3C435A"

    Row {
        anchors.fill: parent
        spacing: 10

        OverviewControls.BaseBalanceLabel {
            id: confirmed_balance
            width: 130
            label_text: "Confirmed balance"
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: control.spacer_color
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: unconfirmed_balance
            width: 140
            label_text: "Unconfirmed balance"
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: control.spacer_color
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: total_balance
            width: 120
            label_text: "Total balance"
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: control.spacer_height
            color: control.spacer_color
            anchors.verticalCenter: parent.verticalCenter
        }

        OverviewControls.BaseBalanceLabel {
            id: used_addresses
            label_text: "#Used addresses"
            value_suffix: ""
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
