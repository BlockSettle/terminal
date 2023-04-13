/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Layouts 1.3

import "BsStyles"

Item {
    id: infoBarRoot
    height: BSSizes.applyScale(30)

    property bool showChangeApplyMessage: false

    RowLayout {
        anchors.fill: parent
        spacing: BSSizes.applyScale(10)

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                visible: infoBarRoot.showChangeApplyMessage
                anchors {
                    fill: parent
                    leftMargin: BSSizes.applyScale(10)
                }
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter

                text: qsTr("Changes will take effect after the application is restarted.")
                color: BSStyle.inputsPendingColor
            }
        }

        Rectangle {
            id: netLabel
            property bool bInitAsTestNet: signerSettings.testNet

            radius: BSSizes.applyScale(5)
            color: bInitAsTestNet ? BSStyle.testnetColor : BSStyle.mainnetColor
            width: BSSizes.applyScale(100)
            height: BSSizes.applyScale(20)
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: netLabel.bInitAsTestNet ? qsTr("Testnet") : qsTr("Mainnet")
                color: netLabel.bInitAsTestNet ? BSStyle.testnetTextColor : BSStyle.mainnetTextColor
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Component.onCompleted: {
                bInitAsTestNet = signerSettings.testNet
            }
        }
    }

    Rectangle {
        height: 1
        width: parent.width
        color: Qt.rgba(1, 1, 1, 0.1)
        anchors.bottom: parent.bottom
    }
}
