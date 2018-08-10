import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

CustomDialog {
    property string walletId
    property string walletName
    property bool   isRootWallet
    property string rootName
    property bool   backup: chkBackup.checked

    id:root
    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (chkConfirm.checked) {
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
                    text:  qsTr("Delete Wallet")

                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                width: parent.width

                CustomLabelValue {
                    Layout.fillWidth: true
                    width: parent.width
                    text:   isRootWallet
                            ? qsTr("Are you sure you wish to irrevocably delete the entire wallet <%1> and all associated wallet files from your computer?").arg(walletName)
                            : ( rootName.length ? qsTr("Are you sure you wish to delete leaf wallet <%1> from HD wallet <%2>?").arg(walletName).arg(rootName)
                                                : qsTr("Are you sure you wish to delete wallet <%1>?").arg(walletName) )
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
                        id:     chkConfirm
                        text:   qsTr("I understand all the risks of wallet deletion")
                    }
                    CustomCheckBox {
                        visible:    isRootWallet
                        id:     chkBackup
                        text:   qsTr("Backup Wallet")
                        checked: isRootWallet
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
                        text:   qsTr("CONFIRM")
                        enabled: chkConfirm.checked

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
                            onClicked: root.close();
                        }
                    }
                }
            }
        }
    }
}
