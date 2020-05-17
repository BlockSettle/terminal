/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.Bip39EntryValidator 1.0


import "../StyledControls"
import "../BsStyles"

ColumnLayout {
    id: topLayout

    property int wordsCount: -1
    property bool accepted: false
    property alias mnemonicSentence: mnemonicTextArea.text

    onWordsCountChanged: {
        header.text = qsTr(String("Enter %1 mnemonic words").arg(wordsCount))
        mnemonicTextArea.clear();
    }

    signal entryComplete()

    function forceActiveFocus() {
        mnemonicTextArea.forceActiveFocus()
    }

    CustomHeader {
        id: header
        Layout.fillWidth: true
        Layout.leftMargin: 5
        Layout.rightMargin: 5
        height: 20
    }

    CustomTextArea {
        id: mnemonicTextArea
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 5
        Layout.rightMargin: 5
        selectByMouse: true
        font: fixedFont
        onTextChanged: {
            accepted = validator.validate(mnemonicTextArea.text);
            if (accepted) {
                entryComplete();
            }
        }

        Bip39EntryValidator {
            id: validator
            wordsCount: topLayout.wordsCount
            Component.onCompleted: {
                validator.initDictionaries(qmlFactory);
            }
        }
    }

    Rectangle {
        color: "transparent"
        height: 15

        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 15

        CustomLabel {
            id: lblResult
            visible: mnemonicTextArea.text.length !== 0
            topPadding: 1
            bottomPadding: 1
            anchors.left: parent.left
            width: mnemonicTextArea.width / 2
            height: 15
            text: topLayout.accepted ? qsTr("Correct mnemonic sentence") : qsTr("Sentence is not completed")
            color: topLayout.accepted ? BSStyle.inputsValidColor : BSStyle.inputsPendingColor
        }
    }
}
