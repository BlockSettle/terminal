import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.1
import Qt.labs.settings 1.0
import com.blocksettle.TXInfo 1.0


ApplicationWindow {
    visible: true
    title: qsTr("BlockSettle Signer")
    width: 640
    height: 480
    id: mainWindow

    background: Rectangle {
        color: "#1c2835"
    }
    overlay.modal: Rectangle {
        color: "#737373"
    }
    overlay.modeless: Rectangle {
        color: "#939393"
    }

    Settings {
        id:         settings
        category:   "GUI"
        property alias x:       mainWindow.x
        property alias y:       mainWindow.y
        property alias width:   mainWindow.width
        property alias height:  mainWindow.height
        property alias tabIdx:  swipeView.currentIndex
    }

    InfoBanner {
        id: ibSuccess
        bgColor:    "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor:    "darkred"
    }

    DirSelectionDialog {
        id:     ldrWoWalletDirDlg
        title:  qsTr("Select watching only wallet target directory")
    }
    DirSelectionDialog {
        id:     ldrDirDlg
        title:  qsTr("Select directory")
    }

    SwipeView {
        id: swipeView
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        StatusPage {
            id: dashboardPage
        }

        SettingsPage {
            id: settingsPage
        }

        AutoSignPage {
            id: autoSignPage
        }

        WalletsPage {
            id: walletsPage
        }
    }

    BSMessageBox {
        id: msgBox
    }

    footer: TabBar {
        id: tabBar
        currentIndex: swipeView.currentIndex
        height: 50
        spacing: 2;
        background: Rectangle {
                color: "transparent"
            }

        CustomTabButton {
            id:     btnStatus
            text:   qsTr("Dashboard")

        }

        CustomTabButton {
            id:     btnSettings
            text:   qsTr("Settings")

        }

        CustomTabButton {
            id:     btnAutoSign
            text:   qsTr("Auto-Sign")

        }

        CustomTabButton {
            id:     btnWallets
            text:   qsTr("Wallets")

        }
    }

    onClosing: {
        settingsPage.storeSettings();
        autoSignPage.storeSettings();
    }

    signal passwordEntered(string walletId, string password, bool cancelledByUser)

    function createPasswordDialog(prompt, txInfo) {
        var dlg = Qt.createQmlObject("PasswordEntryDialog {}", mainWindow, "passwordDlg")
        dlg.prompt = prompt
        dlg.txInfo = txInfo
        dlg.accepted.connect(function() {
            passwordEntered(txInfo.wallet.id, dlg.password, false)
        })
        dlg.rejected.connect(function() {
            passwordEntered(txInfo.wallet.id, '', dlg.cancelledByUser)
        })
        mainWindow.requestActivate()
        dlg.open()
    }

   function raiseWindow() {
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
        mainWindow.flags |= Qt.WindowStaysOnTopHint
        mainWindow.flags &= ~Qt.WindowStaysOnTopHint
   }

   function messageBox(type, title, text, details, parent) {
       msgBox.type = type
       msgBox.titleText = title
       msgBox.text = text
       msgBox.details = details
       msgBox.parent = parent
       msgBox.open()
   }

   function messageBoxInfo(title, text, details, parent) {
       messageBox(BSMessageBox.Type.Info, title, text, details, parent);
   }

   function messageBoxCritical(title, text, details, parent) {
       messageBox(BSMessageBox.Type.Critical, title, text, details, parent);
   }
}
