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
    id: transaction_details

    property var tx: null
    property string walletName: ''
    property string address: ''
    property string txAmount: ''
    property string txDateTime: ''
    property string txType: ''
    property color txTypeColor
    property string txComment: ''
    property color txConfirmationsColor

    width: BSSizes.applyWindowWidthScale(916)
    height: BSSizes.applyWindowHeightScale(718)
    anchors.centerIn: Overlay.overlay

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: BSStyle.popupBackgroundColor
        border.width: BSSizes.applyScale(1)
        border.color: BSStyle.popupBorderColor
        radius: BSSizes.applyScale(14)
    }

    contentItem: Rectangle {
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.topMargin: BSSizes.applyScale(12)
            anchors.leftMargin: BSSizes.applyScale(12)
            anchors.rightMargin: BSSizes.applyScale(12)
            anchors.bottomMargin: BSSizes.applyScale(12)
            spacing: BSSizes.applyScale(14)

            Label {
                text: qsTr("Transaction details")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(20)
                font.family: "Roboto"
                font.weight: Font.Bold
                font.letterSpacing: 0.35
            }

            Grid {
                columns: 2
                rowSpacing: BSSizes.applyScale(8)

                Text {
                    text: qsTr("Hash (RPC byte order)")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Row {
                    Text {
                        text: tx !== null ? tx.txId : ""
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: BSSizes.applyScale(14)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    CopyIconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        onCopy: bsApp.copyAddressToClipboard(tx.txId)
                    }
                }

                Text {
                    text: qsTr("Time")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Text {
                    text: transaction_details.txDateTime
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Height")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Text {
                    text: tx !== null ? tx.height : ""
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Amount")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Text {
                    text: transaction_details.txAmount
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Type")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Text {
                    text: transaction_details.txType !== "" ? transaction_details.txType : "..."
                    color: transaction_details.txTypeColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Virtual size (Bytes)")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: tx !== null ? tx.virtSize : ""
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("sat / virtual byte")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: tx !== null ? tx.feePerByte : ""
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Fee")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: tx !== null ? tx.fee : ""
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Confirmations")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: tx !== null ? tx.nbConf : ""
                    color: txConfirmationsColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Wallet")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: transaction_details.walletName !== "" ? transaction_details.walletName : "..."
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }

                Text {
                    text: qsTr("Address")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Row {
                    Text {
                        text: transaction_details.address
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.pixelSize: BSSizes.applyScale(14)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    CopyIconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        onCopy: bsApp.copyAddressToClipboard(transaction_details.address)
                    }
                }

                Text {
                    text: qsTr("Comment")
                    color: BSStyle.titleTextColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                    width: BSSizes.applyScale(170)
                }
                Label {
                    text: transaction_details.txComment === '' ? '-' : transaction_details.txComment
                    color: BSStyle.textColor
                    font.family: "Roboto"
                    font.pixelSize: BSSizes.applyScale(14)
                }
            }

            Label {
                text: qsTr("Input addresses")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(16)
                font.weight: Font.DemiBold
                font.family: "Roboto"
            }

            Rectangle {
                width: parent.width
                height: BSSizes.applyScale(110)
                color: "transparent"
                radius: BSSizes.applyScale(14)
                border.color: BSStyle.popupBorderColor
                border.width: BSSizes.applyScale(1)

                CustomTableView {
                    width: parent.width - BSSizes.applyScale(20)
                    height: parent.height
                    anchors.centerIn: parent
                    model: tx.inputs

                    copy_button_column_index: 1
                    columnWidths: [0.1, 0.5, 0.2, 0.2]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                }
            }

            Text {
                text: qsTr("Output addresses")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(16)
                font.weight: Font.DemiBold
                font.family: "Roboto"
            }

            Rectangle {
                width: parent.width
                height: BSSizes.applyScale(110)
                color: "transparent"
                radius: BSSizes.applyScale(14)
                border.color: BSStyle.popupBorderColor
                border.width: BSSizes.applyScale(1)

                CustomTableView {
                    width: parent.width - BSSizes.applyScale(20)
                    height: parent.height
                    anchors.centerIn: parent
                    model: tx.outputs

                    copy_button_column_index: 1
                    columnWidths: [0.1, 0.5, 0.2, 0.2]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                }
            }
        }
    }

    CloseIconButton {
        anchors.topMargin: BSSizes.applyScale(5)
        anchors.rightMargin: BSSizes.applyScale(5)
        anchors.right: parent.right
        anchors.top: parent.top

        onClose: transaction_details.close()
    }
}
