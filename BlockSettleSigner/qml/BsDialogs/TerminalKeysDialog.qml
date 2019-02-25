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

    property int inputsWidth_: 250

    title: qsTr("Manage Terminal Keys")
    width: 520
    height: 500
    rejectable: true

    cHeaderItem: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 0
    }

    cContentItem: ColumnLayout {
        spacing: 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 1

        CustomHeader {
            text: qsTr("Add Terminal Public Key")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10

            CustomTextInput {
                id: inputName
                Layout.preferredWidth: 150
                placeholderText: qsTr("Name")
            }
            CustomTextInput {
                id: inputKey
                placeholderText: qsTr("Key")
            }

            CustomButton {
                id: btnAddTerminal
                text: qsTr("Add")
                enabled: inputName.text.length > 0 && inputKey.text.length > 0
                Layout.preferredHeight: inputName.implicitHeight
                onClicked: {
                    signerSettings.trustedTerminals.unshift(inputName.text + ":" + inputKey.text)
                }
            }
        }

        CustomHeader {
            text: qsTr("Trusted Terminals")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        ListView {
            id: terminalsView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: 400
            clip: true
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                active: true
            }

            model: signerSettings.trustedTerminals

            delegate: RowLayout {
                Layout.preferredWidth: terminalsView.width
                Layout.preferredHeight: 30

                CustomLabel {
                    // name
                    text: modelData.split(':')[0]
                    Layout.preferredWidth: 150
                }
                CustomLabel {
                    // key
                    text: modelData.split(':')[1]
                    Layout.preferredWidth: 300
                    wrapMode: Text.WrapAnywhere
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
                        var dlg = JsHelper.messageBox(BSMessageBox.Type.Question, "Manage Terminals", "Remove terminal?")
                        dlg.accepted.connect(function(){
                            signerSettings.trustedTerminals.splice(index, 1)
                        })
                    }
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
                id: btnCancel
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Ok")
                onClicked: {
                    rejectAnimated()
                }
            }
        }
    }
}
