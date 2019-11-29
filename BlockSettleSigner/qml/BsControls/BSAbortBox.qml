/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import "../StyledControls"

BSMessageBox {
    type: BSMessageBox.Type.Question
    property int abortType: BSAbortBox.AbortType.Default

    title: qsTr("Warning")
    customText: textList[abortType]
    customDetails: detailsList[abortType]

    acceptButton.text: qsTr("Abort")

    enum AbortType {
        Default = 0,
        WalletCreation = 1,
        WalletImport = 2
    }

    property variant textList: [
        qsTr("Abort?"),
        qsTr("Abort Wallet Creation?"),
        qsTr("Abort Wallet Import?")
    ]

    property variant detailsList: [
        qsTr(""),
        qsTr("The Wallet will not be created if you abort the procedure. Are you sure you wish to abort the Wallet Creation process?"),
        qsTr("The Wallet will not be imported if you abort the procedure. Are you sure you wish to abort the Wallet Import process?")
    ]

}

