import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../StyledControls"

CustomTitleDialogWindow {
    id: root

    property WalletInfo walletInfo: WalletInfo{}
    property bool   isRootWallet: (walletInfo.rootId == walletInfo.walletId)
    property string rootName
    property bool   backup: chkBackup.checked

    implicitWidth: 400
    implicitHeight: 250
    focus: true
    title: qsTr("Delete Wallet")
    rejectable: true


    cContentItem: ColumnLayout {       
        id: mainLayout
        spacing: 10

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            width: parent.width

            CustomLabelValue {
                Layout.fillWidth: true
                width: parent.width
                text: isRootWallet
                        ? qsTr("Are you sure you wish to irrevocably delete the entire wallet <%1> and all associated wallet files from your computer?").arg(walletInfo.name)
                        : ( rootName.length ? qsTr("Are you sure you wish to delete leaf wallet <%1> from HD wallet <%2>?").arg(walletInfo.name).arg(rootName)
                                            : qsTr("Are you sure you wish to delete wallet <%1>?").arg(walletInfo.name) )
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Column {
                Layout.fillWidth: true

                CustomCheckBox {
                    id: chkConfirm
                    text: qsTr("I understand all the risks of wallet deletion")
                }
                CustomCheckBox {
                    visible: isRootWallet
                    id: chkBackup
                    text: qsTr("Backup Wallet")
                    checked: isRootWallet
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    onClicked: rejectAnimated();
                }
            }

            CustomButtonPrimary {
                text: qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: chkConfirm.checked

                onClicked: {
                    acceptAnimated()
                }
            }
        }
    }

}
