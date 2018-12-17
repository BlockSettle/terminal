import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0

import "StyledControls"

CustomTitleDialogWindow {
    id:root

    property WalletInfo wallet
    property string targetDir
    property string backupFileExt:  "." + (isPrintable ? "pdf" : "wdb")
    property string backupFileName: "backup_wallet_" + wallet.name + "_" + wallet.id + backupFileExt
    property string password
    property bool   isPrintable:    false
    property bool   acceptable:     (wallet.encType === WalletInfo.Unencrypted) || tfPassword.text.length || password.length
    property AuthSignWalletObject  authSign

    title: qsTr("Backup Private Key for Wallet %1").arg(wallet.name)
    rejectable: true

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Auth) {
            authSign = authProxy.signWallet(wallet.encKey, qsTr("Backup wallet %1").arg(wallet.name),
                                         wallet.rootId)

            authSign.success.connect(function(key) {
                password = key
                labelAuthStatus.text = qsTr("Password ok")
            })
            authSign.error.connect(function(text) {
                root.rejectAnimated()
            })
        }
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible:    wallet.encType === WalletInfo.Password
                elide: Label.ElideRight
                text: qsTr("Password:")
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfPassword
                visible:    wallet.encType === WalletInfo.Password
                focus: true
                placeholderText: qsTr("Wallet password")
                echoMode: TextField.Password
                Layout.fillWidth: true
                onTextChanged: {
                    acceptable = (text.length > 0)
                }
                onAccepted: {
                    if (text && text.length > 0) {
                        acceptAnimated()
                    }
                }
            }

            CustomLabel {
                id: labelAuth
                visible: wallet.encType === WalletInfo.Auth
                text: qsTr("Sign with Auth eID")
            }
            CustomLabel {
                id: labelAuthStatus
                visible: wallet.encType === WalletInfo.Auth
                text: authSign.status
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
                text: qsTr("Type of backup file")
                verticalAlignment: Text.AlignTop
                Layout.fillHeight: true
            }
            Column {
                Layout.fillWidth: true

                CustomRadioButton {
                    text: qsTr("Digital backup file")
                    checked:    !isPrintable
                    onClicked: {
                        isPrintable = false
                    }
                }
                CustomRadioButton {
                    text: qsTr("Paper backup (PDF file)")
                    checked: isPrintable
                    onClicked: {
                        isPrintable = true
                    }
                }
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
                text:   qsTr("Backup file")
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            CustomLabelValue {
                text:   qsTr("%1/%2").arg(targetDir).arg(backupFileName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.preferredWidth: 300
            }

        }
        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10


            CustomButton {
                text:   qsTr("Select Target Dir")
                Layout.minimumWidth: 80
                Layout.preferredWidth: 80
                Layout.maximumWidth: 80
                Layout.maximumHeight: 25
                Layout.leftMargin: 110 + 5
                onClicked: {
                    if (!ldrDirDlg.item) {
                        ldrDirDlg.active = true
                    }
                    ldrDirDlg.startFromFolder = targetDir
                    ldrDirDlg.item.accepted.connect(function() {
                        targetDir = ldrDirDlg.dir
                    })
                    ldrDirDlg.item.open();
                }
            }
        }

    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true
            id: rowButtons

            CustomButton {
                text:   qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                enabled: acceptable
                text:   qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    acceptAnimated()
                }
            }
        }
    }

    onAccepted: {
        if (wallet.encType === WalletInfo.Password) {
            password = tfPassword.text
        }
    }

    onRejected: {
        authSign.cancel();
    }
}
