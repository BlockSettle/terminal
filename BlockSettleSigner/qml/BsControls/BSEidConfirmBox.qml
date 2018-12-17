import "../StyledControls"

BSMessageBox {
    title: qsTr("Notice!")
    customText: qsTr("PROTECT YOUR ROOT PRIVATE KEY!")
    customDetails: qsTr("No one can help you recover your bitcoins if you forget your wallet passphrase and you don't have your Root Private Key (RPK) backup! A backup of the RPK protects your wallet forever against hard drive loss and loss of your wallet passphrase. The RPK backup also protects you from wallet theft if the wallet was encrypted and the RPK backup wasn't stolen. Please make a backup of the RPK and keep it in a secure place.\n\nPlease approve an Auth eID request one more time to indicate that you are aware of the risks of losing your passphrase!")
    cancelButtonVisible: false
    rejectable: true
}

