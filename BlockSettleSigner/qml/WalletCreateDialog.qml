import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.4
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.MobileClient 1.0

import "bscontrols"

CustomDialog {
    id:root
    width: 400
    // setting the height of this windows based on which page is currently visible, using numbers instead of enum to keep it short
    height: curPage === 1 ? mainLayout.implicitHeight : authSignPage.implicitHeight
    property bool primaryWalletExists: false
    property string password
    property bool isPrimary:    false
    property WalletSeed seed
    property int encType
    property string encKey
    property AuthSignWalletObject  authSign
    property bool acceptable:   tfName.text.length && (newPasswordWithConfirm.acceptableInput || textInputEmail.text)
    property int inputLabelsWidth: 110
    property int curPage: WalletCreateDialog.Page.Main
    property bool authNoticeShown: false
    property int countDown: 120

    Component.onCompleted: {
        tfName.text = qsTr("Wallet #%1").arg(walletsProxy.walletNames.length + 1);

    }

    onCurPageChanged: {
        if (curPage === WalletCreateDialog.Page.AuthSignPage) {
            authProgress.value = countDown = 120.0
            authTimer.start()
        }
        else {
            authTimer.stop()
        }
    }
    Timer {
        id: authTimer
        interval: 1000; running: false; repeat: true
        onTriggered: {
            authProgress.value = countDown--
            if (countDown == 0) {
                authTimer.stop()
                curPage = WalletCreateDialog.Page.Main
            }
        }
    }

    enum Page {
        Main = 1,
        AuthSignPage = 2
    }

    // this function is called by abort message box in WalletsPage
    function abort() {
        reject()
    }

    // handles accept signal from msgBox which displays password and auth eid notice
    function msgBoxAccept() {
        // accept only when using password authentication
        if (rbPassword.checked) {
            accept()
        }
    }

    onOpened: {
        abortBox.bWalletCreate = true
        abortBox.accepted.connect(abort)
        noticeBox.accepted.connect(msgBoxAccept)
    }
    onClosed: {
        abortBox.accepted.disconnect(abort)
        noticeBox.accepted.disconnect(msgBoxAccept)
    }

    contentItem: FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (acceptable) {
                    accept()
                }

                event.accepted = true;
            } else if (event.key === Qt.Key_Escape) {
                abortBox.open()
                event.accepted = true
            }
        }

        ColumnLayout {
            anchors.fill: parent
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10
            width: parent.width
            id: mainLayout
            visible: curPage === WalletCreateDialog.Page.Main

            RowLayout{
                CustomHeaderPanel{
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:  qsTr("Create New Wallet")
                }
            }
            CustomHeader {
                id: headerText
                text:   qsTr("Wallet Details")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomRadioButton {
                    id: rbMainNet
                    text:   qsTr("MainNet")
                    Layout.leftMargin: inputLabelsWidth
                    checked: seed ? seed.mainNet : false
                    onClicked: {
                        seed.mainNet = true
                    }
                }
                CustomRadioButton {
                    id: rbTestNet
                    text:   qsTr("TestNet")
                    checked: seed ? seed.testNet : false
                    onClicked: {
                        seed.testNet = true
                    }
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Wallet ID")
                }
                CustomLabel {
                    Layout.fillWidth: true
                    text: seed ? seed.walletId : qsTr("")
                }
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Name")
                }
                CustomTextInput {
                    id: tfName
                    Layout.fillWidth: true
                    selectByMouse: true
                    focus: true
                    onEditingFinished: {
                        seed.walletName = tfName.text
                    }
                }
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Description")
                }
                CustomTextInput {
                    id: tfDesc
                    Layout.fillWidth: true
                    selectByMouse: true
                    validator: RegExpValidator {
                        regExp: /^[^\\\\/?:*<>|]*$/
                    }
                    onEditingFinished: {
                        seed.walletDesc = tfDesc.text
                    }
                }
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomCheckBox {
                    id: cbPrimary
                    Layout.fillWidth: true
                    Layout.leftMargin: inputLabelsWidth + 5
                    enabled: !primaryWalletExists
                    text:   qsTr("Primary Wallet")
                    //ToolTip.text: qsTr("A primary Wallet already exists, wallet will be created as regular wallet.")
                    //ToolTip.delay: 1000
                    //ToolTip.timeout: 5000
                    //ToolTip.visible: hovered
                }
            }
            CustomHeader {
                id: headerText2
                text:   qsTr("Create Wallet Keys")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomRadioButton {
                    id: rbPassword
                    text:   qsTr("Password")
                    checked:    true
                    Layout.leftMargin: inputLabelsWidth
                }
                CustomRadioButton {
                    id: rbAuth
                    text:   qsTr("Auth eID")
                    onCheckedChanged: {
                        if (checked == true && !authNoticeShown) {
                            // setting to true and then false to properly size
                            // the message box, otherwise sometimes the size is not correct
                            noticeBox.passwordNotice = true
                            noticeBox.passwordNotice = false
                            noticeBox.open()
                            authNoticeShown = true // make sure the notice is only shown once
                        }
                    }
                }
            }

            BSConfirmedPasswordInput {
                id: newPasswordWithConfirm
                visible:    rbPassword.checked
                columnSpacing: 10
                passwordLabelTxt: qsTr("Password")
                passwordInputPlaceholder: qsTr("Wallet Password")
                confirmLabelTxt: qsTr("Confirm Password")
                confirmInputPlaceholder: qsTr("Confirm Wallet Password")
            }

            RowLayout {
                id: authLayout
                visible: rbAuth.checked
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Auth eID email")
                }
                CustomTextInput {
                    id: textInputEmail
                    Layout.fillWidth: true
                    selectByMouse: true
                    focus: true
                    onEditingFinished: {
                        seed.walletName = tfName.text
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
            }

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: root.width
                id: rowButtons

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10
                    width: parent.width - buttonRowLeft - 5
                    LayoutMirroring.enabled: true
                    LayoutMirroring.childrenInherit: true
                    anchors.left: parent.left   // anchor left becomes right


                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text:   qsTr("Continue")
                        enabled:    acceptable
                        onClicked: {
                            if (rbPassword.checked) {
                                noticeBox.passwordNotice = true
                                noticeBox.password = newPasswordWithConfirm.text
                                noticeBox.open()
                            }
                            else {
                                encType = WalletInfo.Auth
                                curPage = WalletCreateDialog.Page.AuthSignPage
                                // the Auth eID sign process should start here

                                authSign = authProxy.signWallet(MobileClient.ActivateWallet,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                      seed.walletId, "")
                                //btnAuth.enabled = false
                                authSign.succeeded.connect(function(encKey_, password_) {
                                    authSign = null
                                    encKey = encKey_
                                    seed.password = password_
                                    acceptable = true
                                    //text = qsTr("Successfully signed")
                                    //text = qsTr("Verify")

                                    msgBox.usePassword = false
                                    msgBox.rejectButtonVisible = false
                                    messageBoxInfo(qsTr("Notice!")
                                                   , qsTr("PROTECT YOUR ROOT PRIVATE KEY!")
                                                   , qsTr("No one can help you recover your bitcoins if you forget your wallet passphrase and you don't have your Root Private Key (RPK) backup! A backup of the RPK protects your wallet forever against hard drive loss and loss of your wallet passphrase. The RPK backup also protects you from wallet theft if the wallet was encrypted and the RPK backup wasn't stolen. Please make a backup of the RPK and keep it in a secure place.\n\nPlease approve an Auth eID request one more time to indicate that you are aware of the risks of losing your passphrase!"))

                                    function verifyWalletKey() {
                                        msgBox.accepted.disconnect(verifyWalletKey)
                                        authSign = authProxy.signWallet(MobileClient.VerifyWalletKey,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                              seed.walletId, encKey)

                                        authSign.succeeded.connect(function(encKey_, password_) {
                                            authSign = null
                                            accept()
                                        })
                                    }
                                    msgBox.accepted.connect(verifyWalletKey)
                                })
                                authSign.failed.connect(function(failed_text) {
                                    authSign = null
                                    btnAuth.enabled = textInputEmail.text.length
                                })
                            }
                        }
                    }
                }

                Flow {
                    id: buttonRowLeft
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Cancel")
                        onClicked: {
                            abortBox.open()
                        }
                    }
                }
            }
        }

        // this is the Auth eID sign status page
        ColumnLayout {
            anchors.fill: parent
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10
            //width: parent.width
            visible: curPage === WalletCreateDialog.Page.AuthSignPage
            id: authSignPage

            RowLayout{
                CustomHeaderPanel{
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:  qsTr("Sign With Auth eID")
                }
            }
            CustomLabel {
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                horizontalAlignment: Qt.AlignCenter
                text: qsTr("Activate Auth eID signing\nWallet ID: %1\n\n(%2)").arg(seed ? seed.walletId : qsTr("")).arg(encKey)
            }

            CustomProgressBar {
                id: authProgress
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                from: 0.0
                to: 120.0
                value: 120.0
            }
            CustomLabel {
                id: seconds
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                horizontalAlignment: Qt.AlignRight
                text: qsTr("%1 seconds left").arg(countDown)
            }

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: root.width

                Flow {
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Cancel")
                        onClicked: {
                            curPage = WalletCreateDialog.Page.Main
                        }
                    }
                }
            }

        }
    }

    function toHex(str) {
        var hex = '';
        for(var i = 0; i < str.length; i++) {
            hex += ''+str.charCodeAt(i).toString(16);
        }
        return hex;
    }

    onAccepted: {
        seed.walletName = tfName.text
        seed.walletDesc = tfDesc.text
        isPrimary = cbPrimary.checked
        if (rbPassword.checked) {
            seed.encType = WalletInfo.Password
            encType = WalletInfo.Password
            password = newPasswordWithConfirm.text
        }
        else {
            seed.encType = WalletInfo.Auth
            seed.encKey = encKey
        }
    }

    onRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }
}
