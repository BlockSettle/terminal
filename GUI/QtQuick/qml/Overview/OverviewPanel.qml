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

    width: 1200
    height: 788
    color: "#191E2A"

    signal copyWallet(var id)
    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)

    Column {
        anchors.margins: 20
        anchors.fill: parent
        spacing: 0

        OverviewControls.OverviewWalletBar {
            id: overview_panel
            width: parent.width
            height: 100

            onRequestWalletProperties: control.requestWalletProperties()
            onCreateNewWallet: control.createNewWallet()
            onWalletIndexChanged: control.walletIndexChanged(index)
        }

        Rectangle {
            height: (parent.height - overview_panel.height) * 0.6
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter

            radius: 16
            color: "#333C435A"
            border.width: 1
            border.color: "#3C435A"

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                Text {
                    text: "Addresses"
                    color: "#FFFFFF"
                    font.pixelSize: 19
                    font.family: "Roboto"
                    font.weight: Font.DemiBold
                    
                }

                OverviewControls.AddressesTableView {
                    width: parent.width
                    height: parent.height - 40

                    header_background_color: "transparent"
                    cell_background_color: "transparent"

                    model: addressListModel

                    onCopyRequested: control.copyWallet(id)
                }
            }
        }

        Rectangle {
            color: "transparent"
            width: parent.width
            height: (parent.height - overview_panel.height) * 0.4

            Column {
                anchors.fill: parent
                anchors.topMargin: 20
                spacing: 10

                Text {
                    text: "Non-settled Transactions"
                    color: "#FFFFFF"
                    font.pixelSize: 19
                    font.family: "Roboto"
                    font.weight: Font.DemiBold
                    
                }

                OverviewControls.AddressesTableView {
                    width: parent.width
                    height: parent.height - 40

                    has_copy: false
                    header_background_color: "transparent"
                    cell_background_color: "transparent"

                    columnWidths: [150, 150, 150, 150, 150, 150, 150, 150]
                    model: pendingTxListModel
                }
            }
        }
    }
}
