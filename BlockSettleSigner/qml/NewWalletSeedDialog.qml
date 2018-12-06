﻿
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.QmlPdfBackup 1.0
import com.blocksettle.WalletSeed 1.0

import "bscontrols"

CustomDialog {
    implicitWidth: curPage == 1 ? mainWindow.width * 0.8 : 400
    implicitHeight: curPage == 1 ? mainWindow.height * 0.98 : 265
    id: root
    property int curPage: 1
    property bool acceptable: (curPage == 1 || seedMatch)
    property bool seedMatch: false

    InfoBanner {
        id: error
        bgColor:    "darkred"
    }

    Connections {
        target: newWalletSeed

        onUnableToPrint: {
            error.displayMessage(qsTr("No printer is installed."))
        }

        onFailedToSave: {
            error.displayMessage(qsTr("Failed to save backup file %1").arg(filePath))
        }
    }

    // this function is called by abort message box in WalletsPage
    function abort() {
        reject()
    }

    onOpened: {
        abortBox.accepted.connect(abort)
    }
    onClosed: {
        abortBox.accepted.disconnect(abort)
    }

    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                accept();
                event.accepted = true;
            } else if (event.key === Qt.Key_Escape) {
                abortBox.open()
                event.accepted = true;
            }
        }

        ColumnLayout {
            spacing: 10
            width: parent.width
            id: mainLayout

            RowLayout{
                CustomHeaderPanel{
                    id: headerText
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:  curPage == 1 ? qsTr("Save your Root Private Key") : qsTr("Confirm Seed")
                }
            }
            CustomLabel {
                text: qsTr("Printing this sheet protects all previous and future addresses generated by this wallet! You can copy the “root key” by hand if a working printer is not available. Please make sure that all data lines contain 9 columns of 4 characters each.\n\nRemember to secure your backup in an offline environment. The backup is uncrypted and will allow anyone who holds it to recover the entire wallet.")
                Layout.leftMargin: 15
                Layout.rightMargin: 15
                Layout.fillWidth: true
                id: label
                horizontalAlignment: Qt.AlignLeft
                visible: curPage == 1
            }

            ScrollView {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: mainLayout.width * 0.95
                Layout.preferredHeight: root.height - (headerText.height+5) - label.height -
                                        rowButtons.height - mainLayout.spacing * 3
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                clip: true
                id: scroll
                contentWidth: width
                contentHeight: pdf.preferedHeight
                visible: curPage == 1

                QmlPdfBackup {
                    anchors.fill: parent;
                    walletId: newWalletSeed.walletId
                    part1: newWalletSeed.part1
                    part2: newWalletSeed.part2
                    id: pdf
                }
            }


            CustomLabel {
                text: qsTr("Your seed is important! If you lose your seed, your bitcoin assets will be permanently lost.\nTo make sure that you have properly saved your seed, please retype it here.")
                id: labelVerify
                horizontalAlignment: Qt.AlignLeft
                Layout.fillWidth: true
                Layout.leftMargin: 15
                Layout.rightMargin: 15
                visible: curPage == 2
            }
            BSEasyCodeInput {
                id: rootKeyInput
                visible: curPage == 2
                sectionHeaderVisible: false
                line1LabelTxt: qsTr("Line 1")
                line2LabelTxt: qsTr("Line 2")
                onAcceptableInputChanged: {
                    if (acceptableInput) {
                        // seed keys are valid so compare them against the newWalletSeed
                        if ((newWalletSeed.part1 + "\n" + newWalletSeed.part2) === privateRootKey) {
                            seedMatch = true
                        }
                        else {
                            seedMatch = false

                            // DONT COMMIT!!! ONLY FOR TESTING!!!
                            //seedMatch = true
                        }
                    }
                }
            }

            CustomButtonBar {
                implicitHeight: childrenRect.height
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom
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
                        id: btnContinue
                        Layout.fillWidth: true
                        text:   qsTr("Continue")
                        enabled: acceptable
                        onClicked: {
                            if (curPage == 1) {
                                curPage = 2;
                            } else if (curPage == 2) {
                                accept();
                            }
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Print")
                        visible: curPage == 1
                        onClicked: {
                            newWalletSeed.print();
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Save")
                        visible: curPage == 1
                        onClicked: {
                            newWalletSeed.save();
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
                            abortBox.open()
                        }
                    }
                }
            }
        }
    }
}
