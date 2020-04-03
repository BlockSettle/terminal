/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.4
import Qt.labs.platform 1.1

import com.blocksettle.TXInfo 1.0
import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.HSMDeviceManager 1.0

import "../StyledControls"
import "../BsControls"
import "../BsStyles"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindowWithExpander {
    property WalletInfo walletInfo: WalletInfo {}
    property TXInfo txInfo: TXInfo {}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}
    property QPasswordData passwordData: QPasswordData {}
    property AuthSignWalletObject authSign: null

    // signingAllowed set in cc or xbt dialog
    property bool signingAllowed: false
    property bool signingIsNotSet: true

    // expanding
    property bool isExpanded: false
    onHeaderButtonClicked: {
        isExpanded = !isExpanded
        signerSettings.defaultSettlDialogExpandedState = isExpanded
    }

    headerButtonText: isExpanded ? "Hide Details" : "Details"

    // rfq details
    readonly property string product: passwordDialogData.Product
    readonly property string productGroup: passwordDialogData.ProductGroup
    readonly property string security: passwordDialogData.Security
    readonly property string side: passwordDialogData.Side
    property string quantity
    property string price: passwordDialogData.Price
    property string priceString
    property string totalValue

    property alias settlementDetailsItem: settlementDetailsContainer.data
    property alias txDetailsItem: txDetailsContainer.data

    readonly property bool acceptable: {
        if (walletInfo.encType === QPasswordData.Password) return tfPassword.text.length > 0
        else if (walletInfo.encType === QPasswordData.Unencrypted) return txInfo.isOfflineTxSigned
        else return true
    }

    readonly property int addressRowHeight: 24

    readonly property int duration: passwordDialogData.DurationTotal / 1000.0
    property real timeLeft: passwordDialogData.DurationLeft / 1000.0
    property int timestamp: passwordDialogData.DurationTimestamp

    readonly property real balanceDivider : qmlFactory.balanceDivider()

    readonly property bool is_sell: side === "SELL"
    readonly property bool is_buy: side === "BUY"

    readonly property string minus_string: ""  // "- "
    readonly property string plus_string: ""   // "+ "

    property string errorMessage
    property string validationTitle

    id: root
    title: isExpanded ? passwordDialogData.Title : ""
    rejectable: true
    width: 500

    function init() {
        if (walletInfo.encType === QPasswordData.Auth) {
            btnConfirm.visible = false
            btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter
        }
        else if (walletInfo.encType === QPasswordData.HSM) {
            hsmDeviceManager.prepareHWDeviceForSign(walletInfo.walletId)
        }

        if (signingAllowed) {
            initAuth()
        }
    }

    function initAuth() {
        if (walletInfo.encType !== QPasswordData.Auth) {
            return
        }

        // auth eid initiated after addresses validated and signingAllowed === true
        // it may occur immediately when sign requested or when update PasswordDialogData received
        if (authSign !== null) {
            return
        }

        let authEidMessage = JsHelper.getAuthEidSettlementInfo(product, priceString, is_sell,
                                                               quantity, totalValue);
        authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SettlementTransaction, walletInfo,
                                                      authEidMessage, duration, timestamp);

        authSign.succeeded.connect(function(encKey, password) {
            if (root) {
                passwordData.encType = QPasswordData.Auth
                passwordData.encKey = encKey
                passwordData.binaryPassword = password
                root.acceptAnimated()
            }
        });
        authSign.userCancelled.connect(function() {
            if (root) rejectWithNoError();
        })
        authSign.canceledByTimeout.connect(function() {
            if (root) rejectWithNoError();
        })
    }

    function getValidationColor(condition) {
       if (signingIsNotSet)
           return BSStyle.inputsPendingColor;
       else if (condition)
           return BSStyle.inputsValidColor;
       else
           return BSStyle.inputsInvalidColor;
    }

    Component.onCompleted: {
        isExpanded = signerSettings.defaultSettlDialogExpandedState
    }

    Connections {
        target: qmlAppObj

        onCancelSignTx: {
            console.log("TxSignSettlementBaseDialog.qml, cancel requested for id=" + settlementId)
            if (txId === passwordDialogData.SettlementId) {
                rejectAnimated()
            }
        }
    }

    onAboutToHide: {
        hsmDeviceManager.releaseDevices();
    }

    Connections {
        target: hsmDeviceManager
        onRequestPinMatrix: JsHelper.showPinMatrix(0);
        onDeviceReady: hsmDeviceManager.signTX(passwordDialogData.TxRequest);
        onDeviceNotFound: hsmDeviceStatus = qsTr("Cannot find device paired with this wallet, device label is :\n") + walletInfo.name;
        onDeviceTxStatusChanged: hsmDeviceStatus = status;
        onTxSigned: {
            passwordData.binaryPassword = signData
            passwordData.encType = QPasswordData.HSM
            acceptAnimated();
        }
    }

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

    onSigningAllowedChanged: {
        if (signingAllowed) {
            initAuth()
        }
         signingIsNotSet = false
    }

    cContentItem: ColumnLayout {
        spacing: 10
        Layout.alignment: Qt.AlignTop

        GridLayout {
            id: gridRfqDetails
            columns: 2
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            rowSpacing: 0

            CustomHeader {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("TRADE SUMMARY")
                Layout.preferredHeight: 25
            }

            // [Simple view] Receive
            CustomLabel {
                visible: !isExpanded
                Layout.fillWidth: true
                text: qsTr("Receive")
            }
            CustomLabelValue {
                visible: !isExpanded
                text: is_sell ? totalValue : quantity
                Layout.alignment: Qt.AlignRight
            }

            // [Simple view] Deliver
            CustomLabel {
                visible: !isExpanded
                Layout.fillWidth: true
                text: qsTr("Deliver")
            }
            CustomLabelValue {
                visible: !isExpanded
                text: is_sell ? quantity : totalValue
                Layout.alignment: Qt.AlignRight
            }

            // Product Group
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Product Group")
            }
            CustomLabelValue {
                visible: isExpanded
                text: productGroup
                Layout.alignment: Qt.AlignRight
            }

            // Security ID
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Security ID")
            }
            CustomLabelValue {
                visible: isExpanded
                text: security
                Layout.alignment: Qt.AlignRight
            }

            // Product
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Product")
            }
            CustomLabelValue {
                visible: isExpanded
                text: product
                Layout.alignment: Qt.AlignRight
            }

            // Side
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Side")
            }
            CustomLabelValue {
                visible: isExpanded
                text: side
                Layout.alignment: Qt.AlignRight
            }

            // Quantity
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Quantity")
            }
            CustomLabelValue {
                visible: isExpanded
                text: quantity
                Layout.alignment: Qt.AlignRight
            }

            // Price
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Price")
            }
            CustomLabelValue {
                text: priceString
                Layout.alignment: Qt.AlignRight
            }

            // Total Value
            CustomLabel {
                visible: isExpanded
                Layout.fillWidth: true
                text: qsTr("Total Value")
            }
            CustomLabelValue {
                visible: isExpanded
                text: totalValue
                Layout.alignment: Qt.AlignRight
            }
        }

        ColumnLayout {
            visible: !isExpanded
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            spacing: 0
            clip: true

            CustomHeader {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                text: validationTitle + qsTr(" Validation")
                Layout.preferredHeight: 25
            }

            RowLayout {
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.fillWidth: true
                    text: validationTitle
                }
                CustomLabelValue {
                    text: if (signingIsNotSet)
                              qsTr("Pending")
                          else if (signingAllowed)
                              qsTr("Valid")
                          else
                              qsTr("Not Valid")

                    color: getValidationColor(signingAllowed)
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        ColumnLayout {
            visible: isExpanded
            id: settlementDetailsContainer
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            spacing: 0
            clip: true
        }

        ColumnLayout {
            visible: isExpanded
            id: txDetailsContainer
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            spacing: 0
            clip: true
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomHeader {
                Layout.fillWidth: true
                text: qsTr("SIGN TRANSACTION")
                Layout.preferredHeight: 25
            }

            RowLayout {
                visible: isExpanded
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    visible: isExpanded
                    Layout.fillWidth: true
                    text: qsTr("Wallet name")
                }
                CustomLabel {
                    visible: isExpanded
                    Layout.alignment: Qt.AlignRight
                    text: walletInfo.name
                }
            }

            RowLayout {
                visible: isExpanded
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    visible: isExpanded
                    Layout.fillWidth: true
                    text: qsTr("Wallet ID")
                }
                CustomLabel {
                    visible: isExpanded
                    Layout.alignment: Qt.AlignRight
                    text: walletInfo.walletId
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                visible: walletInfo.encType === QPasswordData.Auth

                CustomLabelValue {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignCenter
                    text: qsTr("Auth eID")
                    visible: walletInfo.encType === QPasswordData.Auth
                }
                CustomLabel {
                    Layout.alignment: Qt.AlignRight
                    text: walletInfo.email()
                    visible: walletInfo.encType === QPasswordData.Auth
                }
            }

            RowLayout {
                visible: walletInfo.encType === QPasswordData.Password
                spacing: 25
                Layout.fillWidth: true

                CustomLabel {
                    visible: walletInfo.encType === QPasswordData.Password
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    Layout.fillWidth: true
                    text: qsTr("Password")
                }

                CustomPasswordTextInput {
                    id: tfPassword
                    visible: walletInfo.encType === QPasswordData.Password
                    focus: true
                    //placeholderText: qsTr("Password")
                    Layout.fillWidth: true
                    Keys.onEnterPressed: {
                        if (btnConfirm.enabled) btnConfirm.onClicked()
                    }
                    Keys.onReturnPressed: {
                        if (btnConfirm.enabled) btnConfirm.onClicked()
                    }
                }
            }

            ColumnLayout {
                spacing: 5
                Layout.fillWidth: true

                Timer {
                    id: timer
                    interval: 500
                    running: true
                    repeat: true
                    onTriggered: {
                        timeLeft -= 0.5
                        if (timeLeft <= 0) {
                            stop()
                            rejectWithNoError();
                        }
                    }
                    signal expired()
                }

                CustomProgressBar {
                    Layout.minimumHeight: 6
                    Layout.preferredHeight: 6
                    Layout.maximumHeight: 6
                    Layout.topMargin: 10
                    Layout.fillWidth: true
                    to: duration
                    value: timeLeft
                }

                CustomLabelValue {
                    text: if (signingAllowed || signingIsNotSet)
                              return qsTr("%1 seconds left").arg(Math.max(0, timeLeft.toFixed(0)));
                          else
                              return errorMessage;
                    Layout.fillWidth: true
                }
            }
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            id: barFooter
            Layout.fillWidth: true

            CustomButton {
                id: btnCancel
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButton {
                id: btnExportTx
                primary: true
                visible: walletInfo.encType === QPasswordData.Unencrypted
                text: qsTr("EXPORT")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    exportTxDlg.open()
                }

                FileDialog {
                    id: exportTxDlg
                    title: "Save Unsigned Tx"

                    currentFile: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/" + txInfo.getSaveOfflineTxFileName()
                    folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                    fileMode: FileDialog.SaveFile
                    nameFilters: [ "Key files (*.bin)", "All files (*)" ]

                    onAccepted: {
                        txInfo.saveToFile(qmlAppObj.getUrlPath(exportTxDlg.file))
                        btnExportTx.primary = false
                        btnExportTx.visible = false
                        btnImportTx.primary = true
                        btnImportTx.enabled = true
                        btnImportTx.visible = true
                    }
                }
            }

            CustomButton {
                id: btnImportTx
                visible: false
                enabled: false
                text: qsTr("IMPORT")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    importTxDlg.open()
                }

                FileDialog {
                    id: importTxDlg
                    title: "Open Signed Tx"

                    currentFile: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/" + passwordDialogData.SettlementId + ".bin"
                    folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                    fileMode: FileDialog.OpenFile
                    nameFilters: [ "Key files (*.bin)", "All files (*)" ]

                    onAccepted: {
                        result = txInfo.loadSignedTx(qmlAppObj.getUrlPath(importTxDlg.file))
                        if (result) {
                            btnImportTx.primary = false
                            btnImportTx.visible = false
                            btnConfirm.primary = true
                            btnConfirm.visible = true
                        }
                        else {
                            JsHelper.messageBox(BSMessageBox.Type.Warning
                                , qsTr("Signed Transacton Import")
                                , qsTr("Error importing signed transaction")
                                , qsTr("Error while importing signed transaction file.\nComparsion between signed and unsigned transaction failed."))
                        }
                    }
                }
            }

            CustomButton {
                id: btnConfirm
                primary: walletInfo.encType ===  QPasswordData.Unencrypted ? false : true
                visible: walletInfo.encType === QPasswordData.Password
                text: walletInfo.encType === QPasswordData.Unencrypted ?  qsTr("BROADCAST") : qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: signingAllowed && acceptable
                onClicked: {
                    if (walletInfo.encType === QPasswordData.Password) {
                        passwordData.textPassword = tfPassword.text
                        passwordData.encType = QPasswordData.Password
                        acceptAnimated()
                    }
                    else if (walletInfo.encType === QPasswordData.Auth) {
                    }
                    else {
                        passwordData.encType = QPasswordData.Unencrypted
                        passwordData.binaryPassword = txInfo.getSignedTx()
                        acceptAnimated()
                    }
                }
            }
        }
    }
}
