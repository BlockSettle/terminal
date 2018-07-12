import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.FrejaProxy 1.0
import com.blocksettle.FrejaSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0

CustomDialog {
    property WalletInfo wallet
    property string targetDir
    property string backupFileExt:  "." + (isPrintable ? "pdf" : "wdb")
    property string backupFileName: "backup_wallet_" + wallet.name + "_" + wallet.id + backupFileExt
    property string password
    property bool   isPrintable:    false
    property bool   acceptable:     (wallet.encType === WalletInfo.Unencrypted) || tfPassword.text.length || password.length
    property FrejaSignWalletObject  frejaSign

    id:root
    implicitWidth: 400
    implicitHeight: mainLayout.childrenRect.height

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Freja) {
            frejaSign = freja.signWallet(wallet.encKey, qsTr("Backup wallet %1").arg(wallet.name),
                                         wallet.rootId)

            frejaSign.success.connect(function(key) {
                password = key
                labelFrejaStatus.text = qsTr("Password ok")
            })
            frejaSign.error.connect(function(text) {
                root.reject()
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
                root.close();
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
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:   qsTr("Backup Private Key for Wallet %1").arg(wallet.name)

                }
            }

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
                            accept()
                        }
                    }
                }

                CustomLabel {
                    id: labelFreja
                    visible: wallet.encType === WalletInfo.Freja
                    text: qsTr("Sign with Freja eID")
                }
                CustomLabel {
                    id: labelFrejaStatus
                    visible: wallet.encType === WalletInfo.Freja
                    text: frejaSign.status
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
                    text:   qsTr("Type of backup file:")
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
                    text:   qsTr("Backup file:")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    verticalAlignment: Text.AlignTop
                }
                CustomLabelValue {
                    text:   qsTr("%1/%2").arg(targetDir).arg(backupFileName)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
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
                            onClicked: root.reject();
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
            password = toHex(tfPassword.text)
        }
    }

    onRejected: {
        frejaSign.cancel();
    }
}
