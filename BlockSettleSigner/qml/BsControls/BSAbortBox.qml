import "../StyledControls"

BSMessageBox {
    type: BSMessageBox.Type.Question
    property int abortType: BSAbortBox.AbortType.Default

    title: qsTr("Warning")
    customText: textList[abortType]
    customDetails: detailsList[abortType]

    acceptButton.text: qsTr("Abort")

    enum AbortType {
        Default = 0,
        WalletCreation = 1,
        WalletImport = 2
    }

    property variant textList: [
        qsTr("Abort?"),
        qsTr("Abort Wallet Creation?"),
        qsTr("Abort Wallet Import?")
    ]

    property variant detailsList: [
        qsTr(""),
        qsTr("The Wallet will not be created if you don't complete the procedure. Are you sure you want to abort the Wallet Creation process?"),
        qsTr("The Wallet will not be imported if you don't complete the procedure.\n\nAre you sure you want to abort the Wallet Import process?")
    ]

}

