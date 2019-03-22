import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../BsControls"
import "../BsStyles"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomDialog {
    id: changeEncryptionDialog

    property WalletInfo walletInfo : WalletInfo {}
    property QPasswordData newPasswordData: QPasswordData {}
    property QPasswordData oldPasswordData: QPasswordData {}

    property bool acceptableOld : walletInfo.encType === NsWallet.Password ? walletDetailsFrame.password.length : true
    property bool acceptableNewPw : newPasswordInput.acceptableInput
    property bool acceptableNewAuth : textInputEmail.text.length > 3
    property bool acceptableNew : rbPassword.checked ? acceptableNewPw : acceptableNewAuth

    property bool acceptable : {
        if (tabBar.currentIndex === 0) return acceptableOld && acceptableNew
        if (tabBar.currentIndex === 1) return walletInfo.encType === NsWallet.Auth
        if (tabBar.currentIndex === 2) return false
    }

    property int inputsWidth_: 250

    title: qsTr("Manage Encryption")
    width: 400
    height: 450
    rejectable: true

    Connections {
        target: walletInfo
        onWalletChanged: {
            walletDetailsFrame.walletInfo = walletInfo
            devicesView.model = walletInfo.encKeys
        }
    }

    onWalletInfoChanged: {
        // need to update object since bindings working only for basic types
        if (walletInfo !== null) {
            walletDetailsFrame.walletInfo = walletInfo
            devicesView.model = walletInfo.encKeys
        }
    }   

    cHeaderItem: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 0
    }

    cContentItem: ColumnLayout {
        spacing: 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 1

        ColumnLayout {
            TabBar {
                id: tabBar
                spacing: 2
                height: 35

                Layout.fillWidth: true
                position: TabBar.Header

                background: Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                }

                CustomTabButton {
                    id: simpleTabButton
                    text: "Simple"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
                CustomTabButton {
                    id: addTabButton
                    //enabled: walletInfo.encType === NsWallet.Auth
                    text: "Add Device"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
                CustomTabButton {
                    id: deleteTabButton
                    //enabled: walletInfo.encType === NsWallet.Auth
                    text: "Remove Device"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
            }
        }

        StackLayout {
            id: stackView
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true

            Item {
                id: simpleTab
                ColumnLayout {
                    spacing: 10

                    BSWalletDetailsFrame {
                        id: walletDetailsFrame
                        walletInfo: walletInfo
                        inputsWidth: 250
                        nextFocusItem: rbPassword.checked ? newPasswordInput.tfPasswordInput : textInputEmail
                        KeyNavigation.tab: rbPassword.checked ? newPasswordInput.tfPasswordInput : textInputEmail
                        //Keys.onEnterPressed: newPasswordInput.tfPasswordInput.forceActiveFocus()
                        //Keys.onReturnPressed: newPasswordInput.tfPasswordInput.forceActiveFocus()
                    }

                    CustomHeader {
                        text: qsTr("New Encryption")
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

                        CustomLabel {
                            elide: Label.ElideRight
                            text: ""
                            wrapMode: Text.WordWrap
                            Layout.minimumWidth: 110
                            Layout.preferredWidth: 110
                            Layout.maximumWidth: 110
                            Layout.fillWidth: true
                        }

                        CustomRadioButton {
                            id: rbPassword
                            text: qsTr("Password")
                            checked: true
                        }
                        CustomRadioButton {
                            id: rbAuth
                            text: qsTr("Auth eID")
                            checked: false
                            onCheckedChanged: {
                                if (checked) {
                                    // show notice dialog
                                    if (!signerSettings.hideEidInfoBox) {
                                        var noticeEidDialog = Qt.createComponent("../BsControls/BSEidNoticeBox.qml").createObject(mainWindow);
                                        noticeEidDialog.open()
                                    }
                                }
                            }
                        }
                    }

                    BSConfirmedPasswordInput {
                        id: newPasswordInput
                        inputsWidth: inputsWidth_
                        columnSpacing: 10
                        visible: rbPassword.checked
                        passwordLabelTxt: qsTr("New Password")
                        confirmLabelTxt: qsTr("Confirm New")
                        nextFocusItem: btnAccept
                        onConfirmInputEnterPressed: {
                            if (btnAccept.enabled) btnAccept.onClicked()
                        }
                    }

                    RowLayout {
                        visible: rbAuth.checked
                        spacing: 5
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10

                        CustomLabel {
                            Layout.minimumWidth: 110
                            Layout.preferredWidth: 110
                            Layout.maximumWidth: 110
                            Layout.fillWidth: true
                            text: qsTr("Auth eID email")
                        }
                        CustomTextInput {
                            id: textInputEmail
                            Layout.fillWidth: true
                            Layout.minimumWidth: inputsWidth_
                            Layout.preferredWidth: inputsWidth_
                            selectByMouse: true
                            focus: true
                            Keys.onEnterPressed: {
                                if (btnAccept.enabled) btnAccept.onClicked()
                            }
                            Keys.onReturnPressed: {
                                if (btnAccept.enabled) btnAccept.onClicked()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                    }
                }

            }

            ColumnLayout {
                id: addTab
                spacing: 5
                Layout.topMargin: 15
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.fillWidth: true

                CustomLabel {
                    text: "Auth eID disabled"
                    color: BSStyle.textColor
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    visible: walletInfo.encType !== NsWallet.Auth
                    Layout.preferredWidth: changeEncryptionDialog.width - 20
                    horizontalAlignment: Text.AlignHCenter
                    padding: 20
                    topPadding: 30
                }

                CustomLabel {
                    Layout.preferredWidth: changeEncryptionDialog.width - 20
                    horizontalAlignment: Text.AlignHCenter
                    padding: 20
                    wrapMode: Text.WordWrap
                    color: walletInfo.encType === NsWallet.Auth ? BSStyle.labelsTextColor : BSStyle.labelsTextDisabledColor

                    text: "Add the ability to sign transactions from your other Auth eID devices.\
\n\n\n Only one signature from one device will be required to sign requests.\
\n\n\n First you'll have to follow the Add Device instructions in your Auth eID app.\n When completed please proceed here.\
\n\n\n Once you press Add Device your activated Auth eID will receive a signing request for adding device.\
\n Once you sign the request a new signing request will be sent to your new device."
                }

            }

            ColumnLayout {
                id: deleteTab

                spacing: 5
                Layout.topMargin: 15
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.fillWidth: true

                CustomLabel {
                    text: "Auth eID disabled"
                    color: BSStyle.textColor
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    visible: walletInfo.encType !== NsWallet.Auth
                    Layout.preferredWidth: changeEncryptionDialog.width - 20
                    horizontalAlignment: Text.AlignHCenter
                    padding: 20
                    topPadding: 30
                }

                CustomHeader {
                    text: qsTr("Devices")
                    textColor: walletInfo.encType === NsWallet.Auth ? BSStyle.textColor : BSStyle.labelsTextDisabledColor
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    Layout.topMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                ListView {
                    id: devicesView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    height: 400
                    interactive: false

                    model: walletInfo.encKeys

                    delegate: RowLayout {
                        Layout.preferredWidth: devicesView.width
                        Layout.preferredHeight: 30

                        CustomLabel {
                            text: JsHelper.parseEncKeyToDeviceName(modelData)
                            Layout.fillWidth: true
                            Layout.preferredWidth: 250
                        }


                        Button {
                            Layout.alignment: Qt.AlignRight
                            background: Rectangle { color: "transparent" }
                            Image {
                                anchors.fill: parent
                                source: "qrc:/resources/cancel.png"
                            }
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18

                            onClicked: {
                                // remove device
                                if (walletInfo.encKeys.length < 2) {
                                    JsHelper.messageBoxCritical("Wallet encryption", "Can't remove last device")
                                    return
                                }

                                JsHelper.removeEidDevice(index
                                                           , walletInfo
                                                           , function(oldPwEidData){
                                                               var ok = walletsProxy.removeEidDevice(walletInfo.walletId
                                                                                                    , oldPwEidData
                                                                                                    , index)
                                                               var mb = JsHelper.resultBox(BSResultBox.RemoveDevice, ok, walletInfo)
                                                           })
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                }
            }

        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true
            CustomButton {
                id: btnCancel
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                id: btnAccept
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: {
                    if (tabBar.currentIndex === 0) return qsTr("CONFIRM")
                    if (tabBar.currentIndex === 1) return qsTr("ADD")
                    if (tabBar.currentIndex === 2) return qsTr("CONFIRM")
                }
                enabled: acceptable
                onClicked: {
                    var ok = false
                    if (tabBar.currentIndex === 0) {
                        // change password
                        if (rbPassword.checked) {
                            // new auth is Password
                            newPasswordData.textPassword = newPasswordInput.password
                            newPasswordData.encType = NsWallet.Password
                        }

                        if (walletInfo.encType === NsWallet.Auth) {
                            // current auth is eID
                            JsHelper.requesteIdAuth(AutheIDClient.DeactivateWallet
                                                    , walletInfo
                                                    , function(oldPwEidData){
                                                        if (rbPassword.checked) {
                                                            // change to password
                                                            ok = walletsProxy.changePassword(walletInfo.walletId
                                                                                                 , oldPwEidData
                                                                                                 , newPasswordData)
                                                            var mb = JsHelper.resultBox(BSResultBox.EncryptionChange, ok, walletInfo)
                                                            mb.accepted.connect(function(){ acceptAnimated() })
                                                        }
                                                        else {
                                                            // change to another eid account
                                                            JsHelper.requesteIdAuth(AutheIDClient.ActivateWallet
                                                                                    , walletInfo
                                                                                    , function(newPwEidData){
                                                                                        ok = walletsProxy.changePassword(walletInfo.walletId
                                                                                                                             , oldPwEidData
                                                                                                                             , newPwEidData)
                                                                                        var mb = JsHelper.resultBox(BSResultBox.EncryptionChange, ok, walletInfo)
                                                                                        mb.accepted.connect(function(){
                                                                                            //acceptAnimated()
                                                                                            addTabButton.onClicked()
                                                                                        })
                                                                                    }) // function(new passwordData)

                                                        }
                                                    }) // function(old passwordData)
                        }
                        else {
                            // current auth is Password
                            oldPasswordData.textPassword = walletDetailsFrame.password
                            oldPasswordData.encType = NsWallet.Password

                            if (rbPassword.checked) {
                                // new auth is Password
                                ok = walletsProxy.changePassword(walletInfo.walletId
                                                                     , oldPasswordData
                                                                     , newPasswordData)
                                var mb = JsHelper.resultBox(BSResultBox.EncryptionChange, ok, walletInfo)
                                mb.accepted.connect(function(){ acceptAnimated() })
                            }
                            else {
                                // new auth is eID
                                JsHelper.activateeIdAuth(textInputEmail.text
                                                        , walletInfo
                                                        , function(newPwEidData){
                                                            ok = walletsProxy.changePassword(walletInfo.walletId
                                                                                            , oldPasswordData
                                                                                            , newPwEidData)
                                                            var mb = JsHelper.resultBox(BSResultBox.EncryptionChange, ok, walletInfo)
                                                            mb.accepted.connect(function(){
                                                                //acceptAnimated()
                                                                addTabButton.onClicked()
                                                            })
                                                        })
                             }
                        }
                    }
                    else if (tabBar.currentIndex === 1) {
                        // add device
                        // step #1. request old device
                        JsHelper.requesteIdAuth(AutheIDClient.ActivateWalletOldDevice
                                                , walletInfo
                                                , function(oldPwEidData){
                                                    // step #2. add new device
                                                    JsHelper.requesteIdAuth(AutheIDClient.ActivateWalletNewDevice
                                                                            , walletInfo
                                                                            , function(newPwEidData){
                                                                                ok = walletsProxy.addEidDevice(walletInfo.walletId, oldPwEidData, newPwEidData)
                                                                                var mb = JsHelper.resultBox(BSResultBox.AddDevice, ok, walletInfo)
                                                                            })
                                                })

                    }
                }
            }
        }
    }

    onAccepted: {
        walletInfo.destroy()
    }
    onRejected: {
        walletInfo.destroy()
    }
}
