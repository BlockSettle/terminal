import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0

import "bscontrols"

CustomDialog {
    property bool primaryWalletExists: false
    property string password
    property bool isPrimary:    false
    property WalletSeed seed
    property int encType
    property string encKey
    property AuthSignWalletObject  authSign
    property bool acceptable:   tfName.text.length && (newPasswordWithConfirm.acceptableInput || tiAuthId.text)
    property int inputLabelsWidth: 110
    property int curPage: WalletCreateDialog.Page.Main
    property bool authNoticeShown: false
    property int countDown: 120
    id:root
    implicitWidth: 400
    // setting the height of this windows based on which page is currently visible, using numbers instead of enum to keep it short
    height: curPage === 1 ? mainLayout.implicitHeight : (curPage === 2 || curPage === 3) ? passwordNotice.implicitHeight : authSignPage.implicitHeight

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
        PasswordNotice = 2,
        AuthNotice = 3,
        AuthSignPage = 4
    }

    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (acceptable) {
                    accept();
                }

                event.accepted = true;
            } else if (event.key === Qt.Key_Escape) {
                root.close();
                event.accepted = true;
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
                            curPage = WalletCreateDialog.Page.AuthNotice
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
                    id: tiAuthId
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
                                curPage = WalletCreateDialog.Page.PasswordNotice
                            }
                            else {
                                encType = WalletInfo.Auth
                                encKey = tiAuthId.text
                                curPage = WalletCreateDialog.Page.AuthSignPage
                                // the Auth eID sign process should start here
                                /*
                                authSign = auth.signWallet(tiAuthId.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                      seed.walletId)
                                btnAuth.enabled = false
                                authSign.success.connect(function(key) {
                                    acceptable = true
                                    password = key
                                    text = qsTr("Successfully signed")
                                })
                                authSign.error.connect(function(text) {
                                    authSign = null
                                    btnAuth.enabled = tiAuthId.text.length
                                })*/
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
                            onClicked: root.reject();
                        }
                    }
                }
            }
        }

        // this is the Notice message box for both password and auth eid warnings
        ColumnLayout {
            anchors.fill: parent
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10
            width: parent.width
            visible: curPage === WalletCreateDialog.Page.PasswordNotice || curPage === WalletCreateDialog.Page.AuthNotice
            id: passwordNotice

            RowLayout{
                CustomHeaderPanel{
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:  qsTr("Notice!")
                }
            }
            CustomHeader {
                text:   rbPassword.checked ? qsTr("Please take care of your assets!") : qsTr("Signing with Auth eID")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }
            CustomLabel {
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                text: rbPassword.checked ? qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! Your Wallet and any backups are useless if you lose them. \n\nA backup protects your wallet forever, against hard drive loss and losing your passphrase. It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. Please make a backup and keep it in a safe place.\n\nPlease enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")
                                         : qsTr("Once you set Auth eID as signing the signing will be set locally on your mobile device.\n\nIf you lose your phone or uninstall the app you will lose your ability to sign wallet requests.\n\nThis also implies that your Auth eID cannot be hacked as it's only stored on your mobile device.\n\nKeep your backup secure as it protects your wallet forever, against hard drive loss and if you lose your mobile device which is connected to Auth eID.\n\nFor more information please consult with the Getting Started with Auth eID guide.")
            }
            BSTextInput {
                id: passwordNoticeConfirm
                visible: rbPassword.checked
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                focus: true
                echoMode: TextField.Password
                placeholderText: qsTr("Password")
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillHeight: true
            }
            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: root.width

                Flow {
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
                        enabled:    newPasswordWithConfirm.text === passwordNoticeConfirm.text || curPage === WalletCreateDialog.Page.AuthNotice
                        onClicked: {
                            // accept and close this dialog if it's password notice page
                            if (curPage === WalletCreateDialog.Page.PasswordNotice) {
                                accept()
                            }
                            // otherwise return back to WalletCreateDialog
                            else {
                                curPage = WalletCreateDialog.Page.Main
                            }
                        }
                    }
                }
                Flow {
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10

                    CustomButton {
                        visible: rbPassword.checked
                        Layout.fillWidth: true
                        text:   qsTr("Cancel")
                        onClicked: {
                            curPage = WalletCreateDialog.Page.Main
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
            encType = WalletInfo.Password
            password = newPasswordWithConfirm.text
        }
    }

    onRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }
}
