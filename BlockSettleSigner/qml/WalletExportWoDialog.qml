import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0

import "StyledControls"

CustomTitleDialogWindow {
    id: root

    property WalletInfo wallet
    property string woWalletFileName
    property string password
    property bool acceptable: wallet ? (wallet.encType === WalletInfo.Unencrypted) || tfPassword.text.length || password.length : false
    property string exportDir:  Qt.resolvedUrl(".")
    property AuthSignWalletObject authSign

    title: wallet ? qsTr("Export Watching-Only Copy of %1").arg(wallet.name) : ""

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Auth) {
            authSign = authProxy.signWallet(wallet.encKey, qsTr("Export watching-only wallet for %1")
                                         .arg(wallet.name), wallet.rootId)

            authSign.success.connect(function(key) {
                password = key
                labelAuthStatus.text = qsTr("Password ok")
            })
            authSign.error.connect(function(text) {
                rejectAnimated()
            })
        }
    }

    cContentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible: wallet ? wallet.encType === WalletInfo.Password : false
                elide: Label.ElideRight
                text: qsTr("Password")
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfPassword
                visible: wallet ? wallet.encType === WalletInfo.Password : false
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
                visible: wallet ? wallet.encType === WalletInfo.Auth : false
                text: qsTr("Sign with Auth eID")
            }
            CustomLabel {
                id: labelAuthStatus
                visible: wallet ? wallet.encType === WalletInfo.Auth : false
                text: authSign ? authSign.status : ""
            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                text:   qsTr("Export to file:")
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            CustomLabelValue {
                text:   qsTr("%1/%2").arg(exportDir).arg(woWalletFileName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            width: parent.width
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
                    if (!ldrWoWalletDirDlg.item) {
                        ldrWoWalletDirDlg.active = true
                    }
                    ldrWoWalletDirDlg.startFromFolder = exportDir
                    ldrWoWalletDirDlg.item.accepted.connect(function() {
                        exportDir = ldrWoWalletDirDlg.dir
                    })
                    ldrWoWalletDirDlg.item.open();
                }
            }
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true
            CustomButton {
                text:   qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: rejectAnimated()
            }
            CustomButtonPrimary {
                enabled: acceptable
                text:   qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: acceptAnimated()
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
