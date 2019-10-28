import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import Qt.labs.platform 1.1

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

CustomDialog {
    id: changeEncryptionDialog

    property int inputsWidth_: 250

    title: qsTr("Manage Terminal Keys")
    width: 620
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
            text: qsTr("Terminal ID Keys")
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
                    Layout.preferredWidth: 400
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
                        var dlg = JsHelper.messageBox(BSMessageBox.Type.Question, "Manage Terminal ID Keys", "Delete Terminal ID Key?")
                        dlg.bsAccepted.connect(function(){
                            signerSettings.trustedTerminals.splice(index, 1)
                        })
                    }
                }
            }
        }


        CustomHeader {
            text: qsTr("Add Terminal ID Key")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        ColumnLayout {
            Layout.margins: 10
            Layout.fillWidth: true

            CustomTextInput {
                id: inputName
                focus: true
                Layout.preferredWidth: 250
                placeholderText: qsTr("Name")
            }
            RowLayout {
                spacing: 6
                CustomTextInput {
                    id: inputKey
                    Layout.preferredWidth: 250
                    placeholderText: qsTr("Key")
                }

                CustomButton {
                    id: btnImportKey
                    Layout.preferredWidth: 150
                    text: qsTr("Import")
                    Layout.preferredHeight: inputName.implicitHeight
                    onClicked: importKeyDialog.open()

                    FileDialog {
                        id: importKeyDialog
                        visible: false
                        title: qsTr("Import Terminal ID Key")

                        nameFilters: [ "Key files (*.pub)", "All files (*)" ]
                        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)

                        onAccepted: {
                            let key = JsHelper.openTextFile(importKeyDialog.file)
                            inputKey.text = key
                        }
                    }
                }
            }



            CustomButton {
                id: btnAddTerminal
                Layout.preferredWidth: 250
                text: qsTr("Add")
                enabled: inputName.text.length > 0 && inputKey.text.length > 0
                Layout.preferredHeight: inputName.implicitHeight
                onClicked: {
                    signerSettings.trustedTerminals.unshift(inputName.text + ":" + inputKey.text)
                    inputName.clear()
                    inputKey.clear()
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
                primary: true
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
