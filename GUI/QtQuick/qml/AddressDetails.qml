/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import terminal.models 1.0

import "StyledControls"
import "BsStyles"

Popup {
    id: address_details
    property string address: ""
    property string transactions: ""
    property string balance: ""
    property string comment: ""
    property string asset_type: ""
    property string type: ""
    property string wallet: ""

    width: 916
    height: 718
    anchors.centerIn: Overlay.overlay

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: BSStyle.popupBackgroundColor
        border.width: 1
        border.color: BSStyle.popupBorderColor
        radius: 14
    }

    contentItem: Rectangle {
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.topMargin: 12
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 12
            spacing: 20

            Text {
                text: qsTr("Address")
                color: BSStyle.textColor
                font.pixelSize: 20
                font.family: "Roboto"
                font.weight: Font.Bold
                font.letterSpacing: 0.35
            }

            Row {
                spacing: 20

                Rectangle {
                    width: 128
                    height: 128
                    color: "white"
                    radius: 10
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        source: address !== "" ? ("image://QR/" + address_details.address) : ""
                        sourceSize.width: parent.width - parent.radius
                        sourceSize.height: parent.width - parent.radius
                        anchors.centerIn: parent
                    }
                }

                Grid {
                    columns: 2
                    rowSpacing: 8

                    Text {
                        text: qsTr("Transactions")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Text {
                        text: address_details.transactions !== '' ? address_details.transactions : '-' 
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                    }

                    Text {
                        text: qsTr("Wallet")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Text {
                        text: address_details.wallet
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                    }

                    Text {
                        text: qsTr("Address")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Row {
                        Text {
                            text: address_details.address
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        CopyIconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            onCopy: bsApp.copyAddressToClipboard(address_details.address)
                        }
                    }

                    Text {
                        text: qsTr("Address Type/ID")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Row {
                        spacing: 14
                        Text {
                            text: address_details.asset_type
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                        }
                        Text {
                            text: address_details.type
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                        }
                    }

                    Text {
                        text: qsTr("Comment")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Text {
                        text: address_details.comment !== '' ? address_details.comment : '-' 
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                    }

                    Text {
                        text: qsTr("Balance")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                        width: 140
                    }
                    Label {
                        text: address_details.balance !== '' ? address_details.balance : '-' 
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: 14
                    }
                }
            }

            Row {
                spacing: 8

                Label {
                    text: qsTr("Incoming transactions")
                    color: BSStyle.textColor
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: "Roboto"
                }

                Image {
                    width: 9
                    height: 12
                    source: "qrc:/images/down_arrow.svg"
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                width: parent.width
                height: 170
                color: "transparent"
                radius: 14
                border.color: BSStyle.popupBorderColor
                border.width: 1

                CustomTableView {
                    width: parent.width - 20
                    height: parent.height
                    anchors.centerIn: parent

                    model: TransactionForAddressFilterModel {
                        id: incoming_transaction_model
                        positive: true
                        sourceModel: txListByAddrModel
                    }
                    copy_button_column_index: 1
                    columnWidths: [0.17, 0.6, 0.08, 0.15]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                }
            }

            Row {
                spacing: 8

                Label {
                    text: qsTr("Outgoing transactions")
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Image {
                    width: 9
                    height: 12
                    source: "qrc:/images/up_arrow.svg"
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                width: parent.width
                height: 170
                color: "transparent"
                radius: 14
                border.color: BSStyle.popupBorderColor
                border.width: 1

                CustomTableView {
                    width: parent.width - 20
                    height: parent.height
                    anchors.centerIn: parent


                    model: TransactionForAddressFilterModel {
                        id: outgoing_transaction_model
                        positive: false
                        sourceModel: txListByAddrModel
                    }
                    copy_button_column_index: 1
                    columnWidths: [0.17, 0.6, 0.08, 0.15]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                }
            }
        }
    }

    CloseIconButton {
        anchors.right: parent.right
        anchors.top: parent.top

        onClose: address_details.close()
    }
}
