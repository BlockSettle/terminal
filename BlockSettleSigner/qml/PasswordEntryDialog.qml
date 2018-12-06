import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.TXInfo 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0


CustomDialog {
    property string prompt
    property TXInfo txInfo
    property string password
    property bool   acceptable: false
    property bool   cancelledByUser: false
    property AuthSignWalletObject  authSign
    closePolicy: Popup.NoAutoClose
    id: passwordDialog

    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    onTxInfoChanged: {
        if (txInfo.wallet.encType === WalletInfo.Auth) {
            authSign = auth.signWallet(txInfo.wallet.encKey, prompt, txInfo.wallet.rootId)

            authSign.success.connect(function(key) {
                acceptable = true
                password = key
                passwordDialog.accept()
            })
            authSign.error.connect(function(text) {
                passwordDialog.reject()
            })
        }
    }

    Connections {
        target: qmlAppObj

        onCancelSignTx: {
            if (txId === txInfo.txId) {
                passwordDialog.reject();
            }
        }
    }

    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (tfPassword.text.length) {
                    confirmClicked();
                }

                event.accepted = true;
            } else if (event.key === Qt.Key_Escape) {
                passwordDialog.reject();
                event.accepted = true;
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            width: passwordDialog.width
            id: mainLayout

            RowLayout{
                CustomHeaderPanel{
                    Layout.preferredHeight: 40
                    id: panelHeader
                    Layout.fillWidth: true
                    text: qsTr("Wallet Password Confirmation")
                }
            }

            CustomLabel {
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                visible: !txInfo.nbInputs && txInfo.wallet.name.length
                text:   qsTr("Wallet %1").arg(txInfo.wallet.name)
            }

            GridLayout {
                id: gridDashboard
                visible: txInfo.nbInputs
                columns:    2
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                rowSpacing: 0

                CustomHeader {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    text:   qsTr("Details")
                    Layout.preferredHeight: 25
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Sending Wallet")
                }
                CustomLabelValue {
                    text:   txInfo.wallet.name
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("No. of Inputs")
                }
                CustomLabelValue {
                    text:   txInfo.nbInputs
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Receiving Address(es)")
                    verticalAlignment: Text.AlignTop
                    Layout.fillHeight: true
                }
                ColumnLayout{
                    spacing: 0
                    Layout.leftMargin: 0
                    Layout.rightMargin: 0
                    Repeater {
                        model:  txInfo.recvAddresses
                        CustomLabelValue {
                            text:   modelData
                            Layout.alignment: Qt.AlignRight
                        }
                    }
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Transaction Size")
                }
                CustomLabelValue {
                    text:   txInfo.txVirtSize
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Input Amount")
                }
                CustomLabelValue {
                    text:   txInfo.inputAmount.toFixed(8)
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Return Amount")
                }
                CustomLabelValue {
                    text:   txInfo.changeAmount.toFixed(8)
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Transaction Fee")
                }
                CustomLabelValue {
                    text:   txInfo.fee.toFixed(8)
                    Layout.alignment: Qt.AlignRight
                }

                CustomLabel {
                    Layout.fillWidth: true
                    text:   qsTr("Transaction Amount")
                }
                CustomLabelValue {
                    text:   txInfo.total.toFixed(8)
                    Layout.alignment: Qt.AlignRight
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomHeader {
                    Layout.fillWidth: true
                    text:   qsTr("Password Confirmation")
                    Layout.preferredHeight: 25
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    visible: prompt.length
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    Layout.fillWidth: true
                    text: prompt
                    elide: Label.ElideRight
                }

                CustomTextInput {
                    id: tfPassword
                    visible: txInfo.wallet.encType === WalletInfo.Password
                    focus: true
                    placeholderText: qsTr("Password")
                    echoMode: TextField.Password
                    Layout.fillWidth: true
                }

                CustomLabel {
                    id: labelAuth
                    visible: txInfo.wallet.encType === WalletInfo.Auth
                    text: authSign.status
                }
            }

            ColumnLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                Timer {
                    id: timer
                    property real timeLeft: 120
                    interval:   500
                    running:    true
                    repeat:     true
                    onTriggered: {
                        timeLeft -= 0.5
                        if (timeLeft <= 0) {
                            stop()
                            passwordDialog.reject()
                        }
                    }
                    signal expired()
                }

                CustomLabel {
                    text: qsTr("On completion just press [Enter] or [Return]")
                    Layout.fillWidth: true
                }
                CustomLabelValue {
                    text: qsTr("%1 seconds left").arg(timer.timeLeft.toFixed((0)))
                    Layout.fillWidth: true
                }

                CustomProgressBar {
                    Layout.minimumHeight: 6
                    Layout.preferredHeight: 6
                    Layout.maximumHeight: 6
                    Layout.bottomMargin: 10
                    Layout.fillWidth: true
                    to: 120
                    value:  timer.timeLeft
                }
            }

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: passwordDialog.width
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
                        Layout.fillWidth: true
                        text:   qsTr("CONFIRM")
                        enabled: tfPassword.text.length || acceptable
                        id: confirmButton

                        onClicked: {
                            confirmClicked();
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
                            cancelledByUser = true
                            passwordDialog.reject();
                        }
                    }
                }
            }
        }
    }

    function toHex(str) {
        var hex = '';
        for(var i = 0; i < str.length; i++) {
            hex += ''+str.charCodeAt(i).toString(16);
        }
        return hex;
    }

    function confirmClicked() {
        if (txInfo.wallet.encType === WalletInfo.Password) {
            password = toHex(tfPassword.text)
        }

        passwordDialog.accept()
    }
}
