/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.WalletInfo 1.0

import "../StyledControls"
import "../BsStyles"

BSPasswordInput {
    id: root

    property WalletInfo walletInfo: WalletInfo{}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}

    title: passwordDialogData.Title
    autheIDSignType: AutheIDClient.EnableTrading

    CustomHeader {
        text: qsTr("Wallet Details")
        Layout.fillWidth: true
        Layout.preferredHeight: 25
        Layout.topMargin: 5
        Layout.leftMargin: 10
        Layout.rightMargin: 10
    }

    RowLayout {
        spacing: 5
        Layout.topMargin: 5
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        CustomLabel {
            Layout.minimumWidth: 110
            Layout.preferredWidth: 110
            Layout.maximumWidth: 110
            Layout.fillWidth: true
            text: qsTr("Wallet name")
        }
        CustomLabel {
            Layout.fillWidth: true
            Layout.preferredWidth: 110
            text: walletInfo.name
        }
    }

    RowLayout {
        spacing: 5
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        CustomLabel {
            Layout.minimumWidth: 110
            Layout.preferredWidth: 110
            Layout.maximumWidth: 110
            Layout.fillWidth: true
            text: qsTr("Wallet ID")
        }
        CustomLabel {
            Layout.fillWidth: true
            text: walletInfo.walletId
        }
    }
}

