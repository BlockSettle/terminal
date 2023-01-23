import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_continue(signature: var)

    height: 554
    width: 600
    spacing: 0

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 34

        Button {
            id: advanced_but

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            enabled: false
            activeFocusOnTab: true

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: "#7A88B0"


            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: 16
            icon.height: 16

            background: Rectangle {
                implicitWidth: 116
                implicitHeight: 34
                color: "transparent"

                radius: 14

                border.color: "#3C435A"
                border.width: 1

            }
        }

        CustomTitleLabel {
            id: title

            Layout.leftMargin: 104
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : 34
        }

    }

    CustomTextInput {

        id: rec_addr_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 552
        Layout.topMargin: 23

        //aliases
        title_text: qsTr("Receiver address")

        Image {
            id: paste_but

            z: 1

            anchors.top: rec_addr_input.top
            anchors.topMargin: 23
            anchors.right: rec_addr_input.right
            anchors.rightMargin: 23

            source: "qrc:/images/paste_icon.png"
            width: 24
            height: 24

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log("paste clicked")
                }
            }
        }
    }

    CustomTextInput {

        id: amount_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 552
        Layout.topMargin: 10

        //aliases
        title_text: qsTr("Amount")
        input_text: "0"

        input_validator: DoubleValidator{bottom: 0;  decimals: 20;  notation :DoubleValidator.StandardNotation;}

        CustomButton {

            id: max_but

            z: 1

            width: 55
            height: 28
            back_radius: 37

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 23

            text: qsTr("MAX")
            font.pixelSize: 12

            function click_enter() {
                console.log ("MAX clicked")
            }
        }

        Label {

            id: currency

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: max_but.left
            anchors.rightMargin: 16

            text: "BTC"
            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
            color: "#7A88B0"
        }
    }

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 70
        Layout.topMargin: 10


        CustomComboBox {

            id: from_wallet_combo

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: 271
            height: 70

            model: walletBalances

            //aliases
            title_text: qsTr("From Wallet")
            details_text: getWalletData(currentIndex, WalletBalance.TotalRole)

            textRole: "name"
            valueRole: "name"
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
        }


        CustomComboBox {

            id: fee_suggest_combo

            Layout.rightMargin: 24
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            model: feeSuggestions

            //aliases
            title_text: qsTr("Fee Suggestions")

            width: 271
            height: 70

            textRole: "text"
            valueRole: "value"
        }
    }

    CustomTextEdit {

        id: comment_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 90
        Layout.preferredWidth: 552
        Layout.topMargin: 10

        //aliases
        title_text: qsTr("Comment")

        onTabNavigated: continue_but.forceActiveFocus()
        onBackTabNavigated: fee_suggest_combo.forceActiveFocus()
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: continue_but

        width: 552

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Continue")

        Component.onCompleted: {
            continue_but.preferred = true
        }

        function click_enter() {
            layout.sig_continue( bsApp.createTXSignRequest(
                            from_wallet_combo.currentIndex, rec_addr_input.text,
                            parseFloat(from_wallet_combo.details_text), parseFloat(fee_suggest_combo.currentValue),
                            comment_input.text))
        }

    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
    }
}

