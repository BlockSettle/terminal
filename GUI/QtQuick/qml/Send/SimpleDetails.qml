import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.3
import Qt.labs.platform 1.1 as QLP

import "../BsStyles"
import "../StyledControls"

import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_continue(signature: var)
    signal sig_advanced()
    signal import_error()

    height: BSSizes.applyWindowHeightScale(554)
    width: BSSizes.applyWindowWidthScale(600)
    spacing: 0

    property var tempRequest: null

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : BSSizes.applyScale(34)
        Layout.leftMargin: BSSizes.applyScale(20)
        Layout.topMargin:  BSSizes.applyScale(10)

        CustomTitleLabel {
            id: title

            Layout.alignment: Qt.AlingVCenter
            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : BSSizes.applyScale(34)
        }

        Button {
            id: import_transaction_button

            // Layout.rightMargin: BSSizes.applyScale(60)
            Layout.alignment: Qt.AlingVCenter

            activeFocusOnTab: false
            hoverEnabled: true

            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: BSStyle.titleTextColor

            text: qsTr("Import transaction")


            icon.color: "transparent"
            icon.source: "qrc:/images/import_icon.svg"
            icon.width: BSSizes.applyScale(16)
            icon.height: BSSizes.applyScale(16)

            background: Rectangle {
                implicitWidth: BSSizes.applyScale(156)
                implicitHeight: BSSizes.applyScale(34)
                color: "transparent"

                radius: BSSizes.applyScale(14)

                border.color: import_transaction_button.hovered ? BSStyle.comboBoxHoveredBorderColor : BSStyle.defaultBorderColor
                border.width: BSSizes.applyScale(1)

            }

            onClicked: {
                importTransactionFileDialog.open()
            }
            
            FileDialog {
                id: importTransactionFileDialog  
                title: qsTr("Please choose transaction to import")
                folder: shortcuts.documents
                selectFolder: false
                selectExisting: true
                onAccepted: {                    
                    tempRequest = bsApp.importTransaction(importTransactionFileDialog.fileUrl)
                    if (bsApp.isRequestReadyToSend(tempRequest)) {
                        sig_continue(tempRequest)
                    }
                    else {
                        import_error()
                    }
                }
            }
        }

        Button {
            id: advanced_but

            Layout.leftMargin: BSSizes.applyScale(20)
            Layout.rightMargin: BSSizes.applyScale(40)
            Layout.alignment: Qt.AlingVCenter

            activeFocusOnTab: false
            hoverEnabled: true

            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: BSStyle.titleTextColor

            text: qsTr("Advanced")


            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: BSSizes.applyScale(16)
            icon.height: BSSizes.applyScale(16)

            background: Rectangle {
                implicitWidth: BSSizes.applyScale(116)
                implicitHeight: BSSizes.applyScale(34)
                color: "transparent"

                radius: BSSizes.applyScale(14)

                border.color: advanced_but.hovered ? BSStyle.comboBoxHoveredBorderColor : BSStyle.defaultBorderColor
                border.width: BSSizes.applyScale(1)

            }

            onClicked: {
               layout.sig_advanced()
            }
        }
    }

    RecvAddrTextInput {

        id: rec_addr_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(552)
        Layout.topMargin: BSSizes.applyScale(23)

        wallets_current_index: from_wallet_combo.currentIndex

        onFocus_next: {
            amount_input.setActiveFocus()
        }

        function createTempRequest() {
            create_temp_request()
        }
    }

    AmountInput {

        id: amount_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(552)
        Layout.topMargin: BSSizes.applyScale(10)

        onEnterPressed: {
            continue_but.click_enter()
        }
        onReturnPressed: {
            continue_but.click_enter()
        }
    }

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.topMargin: BSSizes.applyScale(10)

        WalletsComboBox {

            id: from_wallet_combo

            Layout.leftMargin: BSSizes.applyScale(24)
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: BSSizes.applyScale(271)

            onActivated: (index_act) => {
                walletBalances.selectedWallet = currentIndex
                create_temp_request()
            }

            Connections {
                target: walletBalances
                function onChanged() {
                    if (layout.visible) {
                        create_temp_request()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: BSSizes.applyScale(70)
        }

        FeeSuggestComboBox {

            id: fee_suggest_combo

            Layout.rightMargin: BSSizes.applyScale(24)
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: BSSizes.applyScale(271)
        }
    }

    CustomTextEdit {

        id: comment_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(90)
        Layout.preferredWidth: BSSizes.applyScale(552)
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        title_text: qsTr("Comment")

        onTabNavigated: continue_but.forceActiveFocus()
        onBackTabNavigated: fee_suggest_combo.forceActiveFocus()
    
        onEnterKeyPressed: {
            event.accepted = true;
            continue_but.forceActiveFocus()
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    QLP.FileDialog {
        id: exportFileDialog  
        title: qsTr("Please choose folder to export transaction")
        defaultSuffix: "bin"
        fileMode: QLP.FileDialog.SaveFile
        onAccepted: {
            bsApp.exportTransaction(exportFileDialog.currentFile, continue_but.prepare_transaction())
        }
    }

    CustomButton {
        id: continue_but

        enabled: rec_addr_input.isValid && rec_addr_input.input_text.length
                 && parseFloat(amount_input.input_text) !== 0 && amount_input.input_text.length

        activeFocusOnTab: continue_but.enabled

        width: BSSizes.applyScale(552)

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: (tempRequest !== null && tempRequest.isWatchingOnly) ? qsTr("Export transaction") : qsTr("Continue")

        preferred: true

        function prepare_transaction() {
            return bsApp.createTXSignRequest(from_wallet_combo.currentIndex
                    , [rec_addr_input.input_text], [parseFloat(amount_input.input_text)]
                    , parseFloat(fee_suggest_combo.edit_value()), comment_input.input_text)
        }

        function click_enter() {
            if (!fee_suggest_combo.edit_value())
            {
                fee_suggest_combo.input_text = fee_suggest_combo.currentText
            }

            if (tempRequest.isWatchingOnly)
            {
                exportFileDialog.currentFile = "file:////" + bsApp.makeExportTransactionFilename(tempRequest)
                exportFileDialog.open()
            }
            else
            {
                layout.sig_continue( prepare_transaction() )
            }
        }
    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
    }

    Connections
    {
        target:tempRequest
        function onTxSignReqChanged ()
        {
            //I dont understand why but acceptableInput dont work...
            var cur_value = parseFloat(amount_input.input_text)
            var bottom = 0
            var top = tempRequest.maxAmount
            if(cur_value < bottom || cur_value > top)
            {
                amount_input.input_text = tempRequest.maxAmount
            }
        }
    }

    function create_temp_request()
    {
        if (rec_addr_input.isValid && rec_addr_input.input_text.length) {
            var fpb = parseFloat(fee_suggest_combo.edit_value())
            tempRequest = bsApp.createTXSignRequest(from_wallet_combo.currentIndex
                        , [rec_addr_input.input_text], [], (fpb > 0) ? fpb : 1.0)
        }
    }

    function init()
    {
        bsApp.requestFeeSuggestions()
        rec_addr_input.setActiveFocus()

        //we need set first time currentIndex to 0
        //only after we will have signal rowchanged
        if (fee_suggest_combo.currentIndex >= 0)
            fee_suggest_combo.currentIndex = 0

        amount_input.input_text = ""
        comment_input.input_text = ""
        rec_addr_input.input_text = ""
    }
}

