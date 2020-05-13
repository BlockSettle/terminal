/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQml.Models 2.3

import com.blocksettle.WalletsViewModel 1.0
import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QmlFactory 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0

import "StyledControls"
import "BsStyles"
import "BsControls"
import "BsDialogs"
import "js/helper.js" as JsHelper

Item {
    id: view

    property alias walletsView: walletsView_
    Connections {
        target: walletsView_.model
        onModelReset: {
            // when model resetted selectionChanged signal is not emitted
            // button states needs to be updated after model reset, this emitted signal will do that
            var idx = walletsView_.model.index(-1,-1);
            walletsView_.selection.currentChanged(idx, idx)
        }
    }

    function getCurrentWalletIdData() {
        let data = {}
        let parent = walletsView_.selection.currentIndex;
        while (!walletsView.model.data(parent, WalletsModel.IsHDRootRole)) {
            parent = walletsView_.model.parent(parent);
        }
        data["rootId"] = walletsView_.model.data(parent, WalletsModel.WalletIdRole)
        return data
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true

        ColumnLayout {
            id: colWallets
            width: view.width
            spacing: 5

            CustomButtonBar {
                id: rowButtons
                implicitHeight: childrenRect.height
                Layout.fillWidth: true

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10
                    width: parent.width

                    property bool enableButtons: walletsView_.selection.hasSelection

                    CustomButton {
                        primary: true
                        width: 150
                        text: qsTr("New")
                        onClicked: {
                            // let user create a new wallet or import one from file
                            var dlgNew = Qt.createComponent("BsDialogs/WalletNewDialog.qml").createObject(mainWindow)
                            dlgNew.bsAccepted.connect(function() {
                                if (dlgNew.type === WalletNewDialog.WalletType.NewWallet) {
                                    var dlgNewSeed = JsHelper.createNewWalletDialog()
                                    dlgNewSeed.fullScreenMode = false
                                } if (dlgNew.type === WalletNewDialog.WalletType.ImportWallet) {
                                    JsHelper.importWalletDialog()
                                } if (dlgNew.type === WalletNewDialog.WalletType.ImportHwWallet) {
                                    JsHelper.importHwWalletDialog()
                                }
                            })
                            dlgNew.open()
                        }
                    }

                    CustomButton {
                        primary: true
                        width: 150

                        text: qsTr("Manage")
                        enabled: buttonRow.enableButtons
                        onClicked: {
                            JsHelper.manageEncryptionDialog(getCurrentWalletIdData())
                        }
                    }

                    CustomButton {
                        primary: true
                        width: 150
                        text: qsTr("Export")
                        enabled: buttonRow.enableButtons
                        onClicked: {
                            JsHelper.backupWalletDialog(getCurrentWalletIdData())
                        }
                    }

                    CustomButton {
                        primary: true
                        width: 150
                        enabled: buttonRow.enableButtons
                        text: qsTr("Delete")
                        onClicked: {
                            JsHelper.deleteWalletDialog(getCurrentWalletIdData())
                        }
                    }
                }
            }

            CustomHeader {
                id: header
                text: qsTr("Wallet List")
                height: 25
                checkable: true
                checked: true
                down: true
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                onClicked: {
                    gridGeneral.state = checked ? "normal" : "hidden"
                    highlighted = !checked
                    down = checked
                }
            }

            WalletsView {
                id: walletsView_
                implicitWidth: view.width
                implicitHeight: view.height - rowButtons.height - header.height - colWallets.spacing * 3
            }
        }
    }
}
