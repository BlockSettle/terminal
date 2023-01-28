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
                    rec_addr_input.input_text = bsApp.pasteTextFromClipboard()
                    amount_input.setActiveFocus()
                }
            }
        }

        onTextChanged : {
            if (rec_addr_input.input_text.length)
            {
                rec_addr_input.isValid = bsApp.validateAddress(rec_addr_input.input_text)
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

//                input_validator: DoubleValidator{
//                    bottom: 0
//                    top: (from_wallet_combo.currentIndex >= 0) ?
//                             parseFloat(getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)) : 0
//                    notation :DoubleValidator.StandardNotation
//                }

        //visual studio crashes when there is input_validator
        //and we change text inside of onTextChanged
        //it is why I have realized my validator inside of onTextChanged
        property string prev_text : ""
        onTextChanged : {

            amount_input.input_text = amount_input.input_text.replace(",", ".")

            if (amount_input.input_text.startsWith("0")
                && !amount_input.input_text.startsWith("0.")
                && amount_input.input_text.length > 1)
            {
                amount_input.input_text = "0."
                        + amount_input.input_text.substring(1, amount_input.input_text.length)
            }
            try {
                var input_number =  Number.fromLocaleString(Qt.locale("en_US"), amount_input.input_text)
            }
            catch (error)
            {
                amount_input.input_text = prev_text
                return
            }
            console.log("start amount test")
            console.log("amout text - " + amount_input.input_text)
            console.log("amout number - " + input_number)
            console.log("prev_textr - " + prev_text)

            var max_value = (from_wallet_combo.currentIndex >= 0) ?
                        parseFloat(getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)) : 0
            console.log("max_value - " + max_value)
            if (input_number < 0 || input_number>max_value)
            {
                amount_input.input_text = prev_text
                return
            }

            prev_text = amount_input.input_text
        }

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
                amount_input.input_text = getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)
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

            Connections
            {
                target:walletBalances
                function onRowCountChanged ()
                {
                    from_wallet_combo.currentIndex = overviewWalletIndex
                }
            }

            onActivated: {
                //I dont understand why but acceptableInput dont work...
                var amount_max = getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)
                var cur_value = parseFloat(amount_input.input_text)
                var bottom = amount_input.input_validator.bottom
                var top = amount_input.input_validator.top
                if(cur_value < bottom || cur_value > top)
                {
                    amount_input.input_text = amount_max
                }
            }
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

            Connections
            {
                target:feeSuggestions
                function onRowCountChanged ()
                {
                    fee_suggest_combo.currentIndex = 0
                }
            }
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

        enabled: rec_addr_input.isValid && rec_addr_input.input_text.length
                 && parseFloat(amount_input.input_text) !== 0 && amount_input.input_text.length

        width: 552

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Continue")

        Component.onCompleted: {
            continue_but.preferred = true
        }

        function click_enter() {
            layout.sig_continue( bsApp.createTXSignRequest(
                            from_wallet_combo.currentIndex, rec_addr_input.input_text,
                            parseFloat(amount_input.input_text), parseFloat(fee_suggest_combo.currentValue),
                            comment_input.input_text))
        }

    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
    }

    function init()
    {
        rec_addr_input.setActiveFocus()

        //we need set first time currentIndex to 0
        //only after we will have signal rowchanged
        if (fee_suggest_combo.currentIndex >= 0)
            fee_suggest_combo.currentIndex = 0
        if (from_wallet_combo.currentIndex >= 0)
            from_wallet_combo.currentIndex = overviewWalletIndex

        amount_input.input_text = ""
        comment_input.input_text = ""
        rec_addr_input.input_text = ""
    }
}

