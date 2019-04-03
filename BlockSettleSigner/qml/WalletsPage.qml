import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
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
import "js/qmlDialogs.js" as QmlDialogs

Item {
    id: view

    property alias walletsView: walletsView_
    Connections {
        target: walletsProxy
        onWalletError: {
            JsHelper.messageBoxCritical(qsTr("Error")
                                        , qsTr("Unable to complete this action.")
                                        , qsTr("%1").arg(errMsg))
        }
    }
    Connections {
        target: walletsView_.model
        onModelReset: {
            // when model resetted selectionChanged signal is not emitted
            // button states needs to be updated after model reset, this emitted signal will do that
            var idx = walletsView_.model.index(-1,-1);
            walletsView_.selection.currentChanged(idx, idx)
        }
    }

    function getCurrentWalletInfo() {
        return qmlFactory.createWalletInfo(getCurrentWalletId())
    }

    function getCurrentWalletId() {
        return walletsView_.model.data(walletsView_.selection.currentIndex, WalletsModel.WalletIdRole)
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

                    CustomButtonPrimary {
                        //Layout.fillWidth: true
                        width: 150
                        text: qsTr("New")
                        onClicked: {
                            // let user create a new wallet or import one from file
                            var dlgNew = Qt.createComponent("BsDialogs/WalletNewDialog.qml").createObject(mainWindow)
                            dlgNew.bsAccepted.connect(function() {
                                if (dlgNew.type === WalletNewDialog.WalletType.NewWallet) {
                                    QmlDialogs.createNewWalletDialog()
//                                    // TODO rename signerSettings.testNet -> signerSettings.isTestNet
//                                    var newSeed = qmlFactory.createSeed(signerSettings.testNet)

//                                    // allow user to save wallet seed lines and then prompt him to enter them for verification
//                                    var dlgNewSeed = Qt.createComponent("BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
//                                    dlgNewSeed.seed = newSeed
//                                    dlgNewSeed.bsAccepted.connect(function() {
//                                        // let user set a password or Auth eID and also name and desc. for the new wallet
//                                        var dlgCreateWallet = Qt.createComponent("BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
//                                        dlgCreateWallet.primaryWalletExists = walletsProxy.primaryWalletExists

//                                        dlgCreateWallet.seed = newSeed
//                                        dlgCreateWallet.open();
//                                    })
//                                    dlgNewSeed.open()
                                }
                                else {
                                    QmlDialogs.importWalletDialog()
//                                    var dlgImp = Qt.createComponent("BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
//                                    dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
//                                    dlgImp.open()
                                }
                            })
                            dlgNew.open()
                        }
                    }

                    CustomButtonPrimary {
                        //Layout.fillWidth: true
                        width: 150

                        text: qsTr("Manage")
                        enabled: JsHelper.isSelectedWalletHdRoot(walletsView_)
                        onClicked: {
                            QmlDialogs.manageEncryptionDialog(getCurrentWalletId())
//                            var dlg = Qt.createComponent("BsDialogs/WalletManageEncryptionDialog.qml").createObject(mainWindow)
//                            dlg.walletInfo = getCurrentWalletInfo()
//                            dlg.open()
                        }
                    }

                    CustomButtonPrimary {
                        //Layout.fillWidth: true
                        width: 150
                        text: qsTr("Export")
                        enabled: JsHelper.isSelectedWalletHdRoot(walletsView_)
                        onClicked: {
                            QmlDialogs.backupWalletDialog(getCurrentWalletId())
//                            var dlg = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
//                            dlg.walletInfo = getCurrentWalletInfo()
//                            dlg.targetDir = signerSettings.dirDocuments
//                            dlg.open()
                        }
                    }

                    CustomButtonPrimary {
                        //Layout.fillWidth: true
                        width: 150
                        enabled: JsHelper.isSelectedWalletHdRoot(walletsView_)
                        text: qsTr("Delete")
                        onClicked: {
                            var walletId = walletsView_.model.data(walletsView_.selection.currentIndex, WalletsModel.WalletIdRole)
                            QmlDialogs.deleteWalletDialog(walletId)
//                            var dlg = Qt.createComponent("BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow)
//                            dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
//                            dlg.rootName = walletsProxy.getRootWalletName(dlg.walletId)
//                            dlg.bsAccepted.connect(function() {
//                                if (dlg.backup) {
//                                    var dlgBkp = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
//                                    dlgBkp.walletInfo = qmlFactory.createWalletInfo(walletId)
//                                    dlgBkp.targetDir = signerSettings.dirDocuments
//                                    dlgBkp.bsAccepted.connect(function() {
//                                        if (walletsProxy.deleteWallet(walletId)) {
//                                            JsHelper.messageBox(BSMessageBox.Type.Success
//                                                                , qsTr("Wallet")
//                                                                , qsTr("Wallet successfully deleted.")
//                                                                , qsTr("Wallet Name: %1\nWallet ID: %2").arg(walletName).arg(walletId))
//                                        }
//                                    })
//                                    dlgBkp.open()
//                                }
//                                else {
//                                    if (walletsProxy.deleteWallet(walletId)) {
//                                        JsHelper.messageBox(BSMessageBox.Type.Success
//                                                            , qsTr("Wallet")
//                                                            , qsTr("Wallet successfully deleted.")
//                                                            , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletName).arg(dlg.walletId))
//                                    }
//                                }
//                            })
//                            dlg.open()
                        }
                    }


//                    CustomButtonPrimary {
//                        Layout.fillWidth: true
//                        text: qsTr("Export WO Wallet")
//                        enabled: JsHelper.isSelectedWalletHdRoot(walletsView_)
//                        onClicked: {
//                            var dlg = Qt.createComponent("BsDialogs/WalletExportWoDialog.qml").createObject(mainWindow)
//                            dlg.walletInfo = getCurrentWalletInfo()
//                            dlg.open()
//                        }
//                    }
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
