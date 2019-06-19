import QtQuick 2.9
import QtQuick.Controls 2.4

import "../BsStyles"
import "../StyledControls"

import com.blocksettle.WalletInfo 1.0

BSMessageBox {
    id: root

    property WalletInfo walletInfo: WalletInfo{walletId: "<>"}
    property bool result_: true

    property variant titles: [
          "Wallet"
        , "Wallet"
        , "Wallet"
        , "Wallet encryption"
        , "Wallet encryption"
        , "Wallet encryption"
        , "Wallet encryption"
    ]

    property variant textsSuccess: [
          "Wallet successfully created"
        , "Wallet successfully imported"
        , "Watching-Only Wallet successfully imported"
        , "Password encryption successful"
        , "Auth eID encryption successful"
        , "Device successfully added"
        , "Device successfully removed"
    ]

    property variant textsFail: [
          "Failed to create new Wallet!"
        , "Failed to import Wallet!"
        , "Failed to import Watching-Only Wallet!"
        , "Failed to set new password!"
        , "Failed to set Auth eID encryption!"
        , "Failed to add new device!"
        , "Failed to remove device!"
    ]

    property variant colors: [BSStyle.dialogTitleWhiteColor, BSStyle.dialogTitleGreenColor, BSStyle.dialogTitleOrangeColor, BSStyle.dialogTitleOrangeColor,  BSStyle.dialogTitleRedColor]
    property int resultType: BSResultBox.ResultType.WalletCreate

    enum ResultType {
        WalletCreate = 0,
        WalletImport = 1,
        WalletImportWo = 2,
        EncryptionChangeToPassword = 3,
        EncryptionChangeToAuth = 4,
        AddDevice = 5,
        RemoveDevice = 6
    }

    title: titles[resultType]
    customDetails: qsTr("Wallet ID: %1\nWallet name: %2").arg(walletInfo.walletId).arg(walletInfo.name)
    customText: result_ ? textsSuccess[resultType] : textsFail[resultType]
    type: result_ ? BSMessageBox.Success : BSMessageBox.Critical
    cancelButtonVisible: false
}

