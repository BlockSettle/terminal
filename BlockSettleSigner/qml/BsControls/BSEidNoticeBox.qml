import "../StyledControls"

BSMessageBox {
    title: qsTr("Notice!")
    customText: qsTr("Please take care of your assets!")
    customDetails: qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! Your Wallet and any backups are useless if you lose them.<br><br>A backup protects your wallet forever, against hard drive loss and losing your passphrase. It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. Please make a backup and keep it in a safe place.<br><br>Please enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")
    cancelButtonVisible: false
    rejectable: true
}

