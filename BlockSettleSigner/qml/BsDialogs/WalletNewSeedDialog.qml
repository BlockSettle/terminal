/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import com.blocksettle.QmlPdfBackup 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.QmlFactory 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property int curPage: 1
    property bool acceptable: (curPage == 1 || seedMatch)
    property bool seedMatch: qmlFactory.isDebugBuild() ? true : rootKeyInput.acceptableInput

    property QSeed seed

    title: curPage === 1 ? qsTr("Save your Root Private Key") : qsTr("Confirm Seed")

    property bool fullScreenMode: true
    fixedHeight: true
    width: calcWidth(curPage)
    height: calcHeight(curPage)


    function calcWidth(page) {
        return page === 1 ? (fullScreenMode ? 640 : mainWindow.width * 0.75) : 470
    }

    function calcHeight(page) {
        return page === 1 ? (fullScreenMode ? 800 : mainWindow.height * 0.98) : 280
    }

    abortConfirmation: true
    abortBoxType: BSAbortBox.WalletCreation

    onEnterPressed: {
        if (btnContinue.enabled) btnContinue.onClicked()
    }

    onSeedChanged: {
        // need to update object since bindings working only for basic types
        pdf.seed = seed
    }

    onCurPageChanged: {
        // Bindings not working here for some reason
        // don't try to use property with width here

        // On some resolutions (Windows + 1366x768) slot CustomDialog.qml::onWidthChanged()
        // is not triggered when dialog size changed by width and height bindings
        // workaround - exlicitly change dialog size
        root.width = calcWidth(curPage)
        root.height = calcHeight(curPage)

        if (curPage == 2) rootKeyInput.forceActiveFocus()
    }

    Connections {
        target: pdf

        onSaveSucceed: function(path){
            JsHelper.messageBox(BSMessageBox.Success, "Create Wallet", "Root Private Key successfully saved", path)
        }
        onSaveFailed: function(path) {
            JsHelper.messageBox(BSMessageBox.Critical, "Create Wallet", "Failed to save Root Private Key", path)
        }

        onPrintSucceed: {
            //JsHelper.messageBox(BSMessageBox.Success, "Create Wallet", "Wallet pdf seed saved")
        }
        onPrintFailed: {
            JsHelper.messageBox(BSMessageBox.Critical, "Create Wallet", "Failed to print seed", "")
        }
    }

    cContentItem: StackLayout {
        Layout.fillWidth: true
        currentIndex: curPage - 1

        id: mainLayout

        ColumnLayout {
            //visible: curPage === 1
            Layout.fillWidth: true

            CustomLabel {
                text: qsTr("Remember to keep one or more copies of your Root Private Key as backup.\
Always keep it safe, anyone with this backup may take control of your wallet.")

                leftPadding: 15
                rightPadding: 15
                Layout.preferredWidth: mainLayout.width * 0.95

                id: label
                horizontalAlignment: Qt.AlignLeft
            }

            ScrollView {
                id: scroll
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: mainLayout.width * 0.95
                Layout.preferredHeight: mainLayout.height * 0.95

                Layout.fillHeight: true
                Layout.fillWidth: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                clip: true
                contentWidth: width
                contentHeight: pdf.preferredHeightForWidth - 50

                onWidthChanged: {
                    pdf.parent.width = scroll.width
                    pdf.parent.height = pdf.preferredHeightForWidth - 50
                }

                QmlPdfBackup {
                    id: pdf
                    anchors.fill: parent
                    seed: root.seed
                }

                Component.onCompleted: {
                    ScrollBar.vertical.position += 0.15
                }
            }
        }


        ColumnLayout {
            Layout.fillWidth: true

            CustomLabel {
                text: qsTr("Your seed is important! If you lose your seed, your bitcoin assets will be permanently lost. \
                            \n\nTo make sure that you have properly saved your seed, please retype it here.")
                id: labelVerify
                horizontalAlignment: Qt.AlignLeft
                Layout.maximumWidth: 470
                leftPadding: 15
                rightPadding: 15
            }
            BSEasyCodeInput {
                id: rootKeyInput
                Layout.maximumWidth: 470
                line1LabelTxt: qsTr("Line 1")
                line2LabelTxt: qsTr("Line 2")
                privateRootKeyToCheck: seed.part1 + "\n" + seed.part2
            }
        }
    }

    cFooterItem: RowLayout {
        width: calcWidth(curPage)
        CustomButtonBar {
            Layout.fillWidth: true
            id: rowButtons

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButton {
                id: btnContinue
                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Continue")
                enabled: acceptable
                onClicked: {
                    if (curPage === 1) {
                        curPage = 2;
                    }
                    else if (curPage === 2) {
                        acceptAnimated();
                    }
                }
            }

            CustomButton {
                id: btnPrint
                anchors.right: btnContinue.left
                anchors.bottom: parent.bottom
                text: qsTr("Print")
                visible: curPage === 1
                onClicked: {
                    pdf.print();
                }
            }

            CustomButton {
                id: btnBack
                anchors.right: btnContinue.left
                anchors.bottom: parent.bottom
                text: qsTr("Back")
                visible: curPage == 2
                onClicked: {
                    curPage = 1
                }
            }

            CustomButton {
                id: btnSave
                anchors.right: btnPrint.left
                anchors.bottom: parent.bottom
                text: qsTr("Save")
                visible: curPage === 1
                onClicked: {
                    pdf.save();
                }
            }
        }
    }

    function applyDialogClosing() {
        JsHelper.openAbortBox(root, abortBoxType);
        return false;
    }
}
