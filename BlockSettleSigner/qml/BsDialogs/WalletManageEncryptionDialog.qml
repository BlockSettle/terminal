import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0

import "../BsControls"
import "../BsStyles"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property WalletInfo walletInfo : WalletInfo {}
    property QPasswordData newPasswordData: QPasswordData {}
    property QPasswordData oldPasswordData: QPasswordData {}

    property bool primaryWalletExists: walletsProxy.primaryWalletExists
    property bool hasCCInfoLoaded: walletsProxy.hasCCInfo

    property bool acceptableOld : walletInfo.encType === QPasswordData.Password ? walletDetailsFrame.password.length : true
    property bool acceptableNewPw : newPasswordInput.acceptableInput
    property bool acceptableNewAuth : textInputEmail.text.length > 3
    property bool acceptableNew : rbPassword.checked ? acceptableNewPw : acceptableNewAuth

    property bool acceptable : {
        if (tabBar.currentIndex === 0) return acceptableOld && acceptableNew
        if (tabBar.currentIndex === 1) return walletInfo.encType === QPasswordData.Auth
        if (tabBar.currentIndex === 2) return false
    }

    property int inputsWidth_: 250

    title: qsTr("Manage Encryption")
    width: 400
    rejectable: true

    Component.onCompleted: {
        simpleTab.forceActiveFocus()
    }

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
        ColumnLayout {
            TabBar {
                id: tabBar
                spacing: 0
                leftPadding: 1
                rightPadding: 1
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
                    //enabled: walletInfo.encType !== QPasswordData.Unencrypted

                    enabled: walletInfo.encType === QPasswordData.Auth
                    text: "Add Device"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
//                CustomTabButton {
//                    id: deleteTabButton
//                    //enabled: walletInfo.encType !== QPasswordData.Unencrypted

//                    enabled: walletInfo.encType === QPasswordData.Auth
//                    text: "Device List"
//                    cText.font.capitalization: Font.MixedCase
//                    implicitHeight: 35
//                }
            }
        }

        StackLayout {
            id: stackView
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true

            FocusScope {
                id: simpleTab
                ColumnLayout {
                    spacing: 10

                    BSWalletDetailsFrame {
                        id: walletDetailsFrame
                        walletInfo: walletInfo
                        inputsWidth: 250
                        nextFocusItem: rbPassword.checked ? newPasswordInput.tfPasswordInput : textInputEmail
                        KeyNavigation.tab: rbPassword.checked ? newPasswordInput.tfPasswordInput : textInputEmail
                    }

                    // do we show Primary Wallet section for wallet encryption dialog?
//                    CustomHeader {
//                        text: qsTr("Primary Wallet")
//                        Layout.fillWidth: true
//                        Layout.preferredHeight: 25
//                        Layout.topMargin: 5
//                        Layout.leftMargin: 10
//                        Layout.rightMargin: 10
//                    }

//                    RowLayout {
//                        spacing: 5
//                        Layout.fillWidth: true
//                        Layout.leftMargin: 10
//                        Layout.rightMargin: 10

//                        CustomCheckBox {
//                            id: cbPrimary
//                            Layout.fillWidth: true
//                            Layout.leftMargin: inputLabelsWidth + 5
//                            text: qsTr("Primary Wallet")
//                            checked: !primaryWalletExists && hasCCInfoLoaded
//                            enabled: hasCCInfoLoaded

//                            ToolTip.text: qsTr("A primary Wallet already exists.")
//                            ToolTip.delay: 150
//                            ToolTip.timeout: 5000
//                            ToolTip.visible: cbPrimary.hovered && primaryWalletExists

//                            // workaround on https://bugreports.qt.io/browse/QTBUG-30801
//                            // enabled: !primaryWalletExists
//                            onCheckedChanged: {
//                                if (primaryWalletExists) cbPrimary.checked = false;
//                            }
//                        }
//                    }


                    CustomHeader {
                        text: qsTr("New Encryption")
                        visible: walletInfo.encType !== QPasswordData.Unencrypted
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Layout.topMargin: 5
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                    }

                    RowLayout {
                        visible: walletInfo.encType !== QPasswordData.Unencrypted
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

                            onCheckedChanged: {
                                if (checked) {
                                    newPasswordInput.tfPasswordInput.focus = true
                                }
                            }
                        }
                        CustomRadioButton {
                            id: rbAuth
                            text: qsTr("Auth eID")
                            checked: false
                            onCheckedChanged: {
                                if (checked) {
                                    textInputEmail.focus = true
                                    // show notice dialog
                                    if (!signerSettings.hideEidInfoBox) {
                                        var noticeEidDialog = Qt.createComponent("../BsControls/BSEidNoticeBox.qml").createObject(mainWindow);
                                        sizeChanged(noticeEidDialog.width, noticeEidDialog.height)

                                        noticeEidDialog.closed.connect(function(){
                                            sizeChanged(root.width, root.height)
                                        })
                                        noticeEidDialog.open()
                                    }
                                }
                            }
                        }
                    }

                    BSConfirmedPasswordInput {
                        id: newPasswordInput
                        visible: rbPassword.checked && walletInfo.encType !== QPasswordData.Unencrypted
                        inputsWidth: inputsWidth_
                        columnSpacing: 10
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
                }

            }

            ColumnLayout {
                id: addTab

                spacing: 5
                Layout.topMargin: 15
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.fillWidth: true

//                CustomLabel {
//                    text: "Auth eID disabled"
//                    color: BSStyle.textColor
//                    Layout.fillWidth: true
//                    Layout.alignment: Qt.AlignTop
//                    visible: walletInfo.encType === QPasswordData.Password
//                    Layout.preferredWidth: root.width - 20
//                    horizontalAlignment: Text.AlignHCenter
//                    padding: 20
//                    topPadding: 30
//                }

                CustomLabel {
                    Layout.preferredWidth: root.width - 20
                    horizontalAlignment: Text.AlignHCenter
                    padding: 20
                    wrapMode: Text.WordWrap
                    color: walletInfo.encType === QPasswordData.Auth ? BSStyle.labelsTextColor : BSStyle.labelsTextDisabledColor

                    text: "Add the ability to sign transactions from your other Auth eID devices.\
\n\n Only one signature from one device will be required to sign requests.\
\n\n First you'll have to follow the Add Device instructions in your Auth eID app.\n When completed please proceed here.\
\n\n Once you press Add Device your activated Auth eID will receive a signing request for adding device.\
\n Once you sign the request a new signing request will be sent to your new device."
                }

                CustomHeader {
                    text: qsTr("Devices")
                    textColor: walletInfo.encType === QPasswordData.Auth ? BSStyle.textColor : BSStyle.labelsTextDisabledColor
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
                    height: 200
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
                                        var ok = walletsProxy.removeEidDevice(walletInfo.walletId, oldPwEidData , index)
                                        var mb = JsHelper.resultBox(BSResultBox.RemoveDevice, ok, walletInfo)
                                })
                            }
                        }
                    }
                }
            }

//            ColumnLayout {
//                id: deleteTab

//                spacing: 5
//                Layout.topMargin: 15
//                Layout.leftMargin: 10
//                Layout.rightMargin: 10
//                Layout.fillWidth: true

//                CustomLabel {
//                    text: "Auth eID disabled"
//                    color: BSStyle.textColor
//                    Layout.fillWidth: true
//                    Layout.alignment: Qt.AlignTop
//                    visible: walletInfo.encType !== QPasswordData.Auth
//                    Layout.preferredWidth: root.width - 20
//                    horizontalAlignment: Text.AlignHCenter
//                    padding: 20
//                    topPadding: 30
//                }

//                CustomHeader {
//                    text: qsTr("Devices")
//                    textColor: walletInfo.encType === QPasswordData.Auth ? BSStyle.textColor : BSStyle.labelsTextDisabledColor
//                    Layout.fillWidth: true
//                    Layout.preferredHeight: 25
//                    Layout.topMargin: 5
//                    Layout.leftMargin: 10
//                    Layout.rightMargin: 10
//                }

//                ListView {
//                    id: devicesView
//                    Layout.fillWidth: true
//                    Layout.fillHeight: true
//                    Layout.leftMargin: 10
//                    Layout.rightMargin: 10
//                    height: 250
//                    interactive: false

//                    model: walletInfo.encKeys

//                    delegate: RowLayout {
//                        Layout.preferredWidth: devicesView.width
//                        Layout.preferredHeight: 30

//                        CustomLabel {
//                            text: JsHelper.parseEncKeyToDeviceName(modelData)
//                            Layout.fillWidth: true
//                            Layout.preferredWidth: 250
//                        }


//                        Button {
//                            Layout.alignment: Qt.AlignRight
//                            background: Rectangle { color: "transparent" }
//                            Image {
//                                anchors.fill: parent
//                                source: "qrc:/resources/cancel.png"
//                            }
//                            Layout.preferredWidth: 18
//                            Layout.preferredHeight: 18

//                            onClicked: {
//                                // remove device
//                                if (walletInfo.encKeys.length < 2) {
//                                    JsHelper.messageBoxCritical("Wallet encryption", "Can't remove last device")
//                                    return
//                                }

//                                JsHelper.removeEidDevice(index
//                                    , walletInfo
//                                    , function(oldPwEidData){
//                                        var ok = walletsProxy.removeEidDevice(walletInfo.walletId, oldPwEidData , index)
//                                        var mb = JsHelper.resultBox(BSResultBox.RemoveDevice, ok, walletInfo)
//                                })
//                            }
//                        }
//                    }
//                }
//            }

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
                    if (tabBar.currentIndex === 0) {
                        // change password
                        if (rbPassword.checked) {
                            // new auth is Password
                            newPasswordData.textPassword = newPasswordInput.password
                            newPasswordData.encType = QPasswordData.Password
                        }

                        if (walletInfo.encType === QPasswordData.Auth) {
                            // current auth is eID
                            JsHelper.requesteIdAuth(AutheIDClient.DeactivateWallet
                                , walletInfo
                                , function(oldPwEidData) {
                                    if (rbPassword.checked) {
                                        // change to password
                                        walletsProxy.changePassword(walletInfo.walletId
                                            , oldPwEidData
                                            , newPasswordData
                                            , function(result) {
                                                var mb = JsHelper.resultBox(BSResultBox.EncryptionChangeToPassword, result, walletInfo)
                                                if (result) {
                                                    mb.bsAccepted.connect(function(){
                                                        acceptAnimated()
                                                    })
                                                }
                                        })
                                    }
                                    else {
                                        // change to another eid account
                                        JsHelper.requesteIdAuth(AutheIDClient.ActivateWallet
                                            , walletInfo
                                            , function(newPwEidData){
                                                walletsProxy.changePassword(walletInfo.walletId
                                                    , oldPwEidData
                                                    , newPwEidData
                                                    , function(result){
                                                        var mb = JsHelper.resultBox(BSResultBox.EncryptionChangeToAuth, result, walletInfo)
                                                        if (result) {
                                                            mb.bsAccepted.connect(function(){
                                                                addTabButton.onClicked()
                                                            })
                                                      }
                                               })
                                        }) // function(new passwordData)
                                    }
                             }) // function(old passwordData)
                        }
                        else {
                            // current auth is Password
                            oldPasswordData.textPassword = walletDetailsFrame.password
                            oldPasswordData.encType = QPasswordData.Password

                            if (rbPassword.checked) {
                                // new auth is Password
                                walletsProxy.changePassword(walletInfo.walletId
                                    , oldPasswordData
                                    , newPasswordData
                                    , function(result){
                                        var mb = JsHelper.resultBox(BSResultBox.EncryptionChangeToPassword, result, walletInfo)
                                         if (result) {
                                             mb.bsAccepted.connect(function(){
                                                 acceptAnimated()
                                             })
                                         }
                                })
                            }
                            else {
                                // new auth is eID
                                JsHelper.activateeIdAuth(textInputEmail.text
                                    , walletInfo
                                    , function(newPwEidData){
                                         walletsProxy.changePassword(walletInfo.walletId
                                             , oldPasswordData
                                             , newPwEidData
                                             , function(result){
                                                 var mb = JsHelper.resultBox(BSResultBox.EncryptionChangeToAuth, result, walletInfo)
                                                 if (result) {
                                                     mb.bsAccepted.connect(function(){
                                                         // addTabButton.onClicked()
                                                         acceptAnimated()
                                                     })
                                                 }
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
