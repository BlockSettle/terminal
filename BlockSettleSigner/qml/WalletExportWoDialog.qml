import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0

CustomDialog {
    property WalletInfo wallet
    property string woWalletFileName
    property string password
    property bool acceptable: wallet ? (wallet.encType === WalletInfo.Unencrypted) || tfPassword.text.length || password.length : false
    property string exportDir:  Qt.resolvedUrl(".")
    property AuthSignWalletObject  authSign

    id: exportWoWalletDialog
    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Auth) {
            authSign = auth.signWallet(wallet.encKey, qsTr("Export watching-only wallet for %1")
                                         .arg(wallet.name), wallet.rootId)

            authSign.success.connect(function(key) {
                password = key
                labelAuthStatus.text = qsTr("Password ok")
            })
            authSign.error.connect(function(text) {
                exportWoWalletDialog.reject()
            })
        }
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
                exportWoWalletDialog.close();
                event.accepted = true;
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            width: parent.width
            id: mainLayout

            RowLayout{
                CustomHeaderPanel{
                    Layout.preferredHeight: 40
                    id: panelHeader
                    Layout.fillWidth: true
                    text:  wallet ? qsTr("Export Watching-Only Copy of %1").arg(wallet.name) : ""
                }
            }

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
                            accept()
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

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: exportWoWalletDialog.width
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
                        enabled: acceptable
                        text:   qsTr("CONFIRM")
                        onClicked: {
                            accept()
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
                            onClicked: exportWoWalletDialog.reject();
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
        if (wallet.encType === WalletInfo.Password) {
            password = tfPassword.text
        }
    }

    onRejected: {
        authSign.cancel();
    }
}
