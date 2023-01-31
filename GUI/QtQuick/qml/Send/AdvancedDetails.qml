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
    signal sig_simple()

    height: 748
    width: 1132
    spacing: 0

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 34

        Button {
            id: simple_but

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            activeFocusOnTab: true

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: "#7A88B0"

            text: qsTr("Simple")

            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: 16
            icon.height: 16

            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 34
                color: "transparent"

                radius: 14

                border.color: "#3C435A"
                border.width: 1

            }

            onClicked: {
               layout.sig_simple()
            }
        }

        CustomTitleLabel {
            id: title

            Layout.leftMargin: 378
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : 34
        }

    }

    RowLayout {

        id: rects_row

        Layout.fillWidth: true
        Layout.preferredHeight : 580
        Layout.topMargin: 20

        spacing: 12

        Rectangle {
            id: inputs_rect

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1

            ColumnLayout  {
                id: inputs_layout

                anchors.fill: parent

                spacing: 0

                Label {
                    id: inputs_title

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    text: qsTr("Inputs")

                    height : 19
                    color: "#E2E7FF"
                    font.pixelSize: 16
                    font.family: "Roboto"
                    font.weight: Font.Medium
                }

                CustomComboBox {

                    id: from_wallet_combo

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
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

                CustomComboBox {

                    id: fee_suggest_combo

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

                    model: feeSuggestions

                    //aliases
                    title_text: qsTr("Fee Suggestions")

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

                Rectangle {

                    height: 1

                    Layout.fillWidth: true
                    Layout.topMargin: 196
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    color: "#3C435A"
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight : true
                }
            }
        }

        Rectangle {
            id: outputs_rect

            Layout.rightMargin: 24
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1

            ColumnLayout  {
                id: outputs_layout

                anchors.fill: parent

                spacing: 0

                Label {
                    id: outputs_title

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    text: qsTr("Outputs")

                    height : 19
                    color: "#E2E7FF"
                    font.pixelSize: 16
                    font.family: "Roboto"
                    font.weight: Font.Medium
                }

                CustomTextInput {

                    id: rec_addr_input

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

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

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

                    //aliases
                    title_text: qsTr("Amount")

                    //app (if was launched from visual studio) crashes when there is input_validator
                    //and we change text inside of onTextEdited
                    //it is why I have realized my validator inside of onTextEdited
                    property string prev_text : ""
                    onTextEdited : {

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

                        var max_value = (from_wallet_combo.currentIndex >= 0) ?
                                    parseFloat(getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)) : 0

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

                CustomTextEdit {

                    id: comment_input

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    Layout.preferredHeight : 90
                    Layout.preferredWidth: 504

                    //aliases
                    title_text: qsTr("Comment")

                    onTabNavigated: continue_but.forceActiveFocus()
                    onBackTabNavigated: fee_suggest_combo.forceActiveFocus()
                }

                CustomButton {
                    id: include_output_but
                    text: qsTr("Include Output")

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    icon.source: "qrc:/images/plus.png"
                    icon.color: include_output_but.enabled ? "#45A6FF" : "#020817"

                    width: 504

                    Component.onCompleted: {
                        confirm_but.preferred = false
                    }

                    function click_enter() {
                        if (!confirm_but.enabled) return

                    }
                }

                Rectangle {

                    height: 1

                    Layout.fillWidth: true
                    Layout.topMargin: 30
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    color: "#3C435A"
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight : true
                }
            }
        }
    }


    Label {
        Layout.fillWidth: true
        Layout.fillHeight : true
    }


    CustomButton {
        id: broadcast_but

        enabled: false

        width: 1084

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Broadcast")

        Component.onCompleted: {
            broadcast_but.preferred = true
        }

        function click_enter() {

        }

    }


    function init()
    {
    }
}
