import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../StyledControls"
import "../BsStyles"


BSMessageBox {
    title: qsTr("Notice!")
    customText: qsTr("Signing with Auth eID")
    customDetails: qsTr("Auth eID is a convenient alternative to passwords. \
Instead of entering a password, BlockSettle Terminal issues a secure notification to mobile devices attached to your wallet's Auth eID account. \
You may then sign wallet-related requests via a press of a button in the Auth eID app on your mobile device(s).\
<br><br>You may add or remove devices to your Auth eID accounts as required by the user, and users may have multiple devices on one account. \
Auth eID requires the user to be vigilant with devices using Auth eID. \
If a device is damaged or lost, the user will be unable to sign Auth eID requests, and the wallet will become unusable.\
<br><br>Auth eID is not a wallet backup! No wallet data is stored with Auth eID. \
Therefore, you must maintain proper backups of your wallet's Root Private Key (RPK). \
In the event that all mobile devices attached to a wallet are damaged or lost, the RPK may be used to create a duplicate wallet. \
You may then attach a password or your Auth eID account to the wallet.\
<br><br>Auth eID, like any software, is susceptible to malware, although keyloggers will serve no purpose. \
Please keep your mobile devices up-to-date with the latest software updates, and never install software offered outside your device's app store.\
<br><br>For more information, please consult:<br><a href=\"https://autheid.com/\"><span style=\"color:white;\">Getting Started With Auth eID</span></a>.")

    acceptable: true
    cancelButtonVisible: false
    width: 500

    messageDialogContentItem: RowLayout {
        Layout.leftMargin: 72
        CustomCheckBox {
            id: cb
            text: qsTr("Don't show this information again")
        }
    }

    onAccepted: {
        if (cb.checked) {
            signerSettings.hideEidInfoBox = true
        }
    }
}

