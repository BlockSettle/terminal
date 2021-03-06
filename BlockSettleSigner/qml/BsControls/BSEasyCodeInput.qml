/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.EasyEncValidator 1.0

import "../StyledControls"
import "../BsStyles"

ColumnLayout {
    id: topLayout

    Layout.topMargin: 2 * columnSpacing
    Layout.bottomMargin: 2 * columnSpacing

    property bool acceptableInput: privateRootKeyToCheck.length ? privateRootKey === privateRootKeyToCheck : acceptableLines
    property bool acceptableLines: (!keyLinesIdentical &&
                                    keyLine1.acceptableInput &&
                                    keyLine2.acceptableInput)

    property alias line1LabelTxt: keyLine1Label.text
    property alias line2LabelTxt: keyLine2Label.text
//    property alias sectionHeaderTxt: sectionHeader.text
//    property alias sectionHeaderVisible: sectionHeader.visible

    property int inputLabelsWidth: 50
    property int rowSpacing: 5
    property int columnSpacing: topLayout.spacing

    property bool keyLinesIdentical: keyLine1.text.length &&
                                     keyLine1.text === keyLine2.text &&
                                     keyLine1.acceptableInput

    property string easyCodePalceholder1: "nnnn nnnn nnnn nnnn nnnn nnnn nnnn nnnn nnnn"
    property string easyCodePalceholder2: "kkkk kkkk kkkk kkkk kkkk kkkk kkkk kkkk kkkk"
    property string identicalLinesErrorMsg: "Same Code Used in Line 1 and Line 2"

    property string  privateRootKey: keyLine1.text + "\n" + keyLine2.text
    property string  privateRootKeyToCheck
    signal entryComplete()

    function forceActiveFocus() {
        keyLine1.forceActiveFocus()
    }

    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.bottomMargin: 5
        CustomLabel {
            id: keyLine1Label
            Layout.fillWidth: true
            Layout.minimumWidth: inputLabelsWidth
            Layout.preferredWidth: inputLabelsWidth
            Layout.maximumWidth: inputLabelsWidth
            text: qsTr("Recovery key Line 1:")
        }

        CustomTextInput {
            id: keyLine1
            Layout.fillWidth: true
            selectByMouse: true
            font: fixedFont
            validator: EasyEncValidator {
                id: line1Validator;
                name: qsTr("Line 1")
            }
            onAcceptableInputChanged: {
                if (acceptableInput && !keyLine2.acceptableInput) {
                    keyLine2.forceActiveFocus();
                }
            }
            onEditingFinished: {
                if (acceptableInput && keyLine2.acceptableInput) {
                    entryComplete()
                }
            }
        }
    }

    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        CustomLabel {
            id: keyLine2Label
            Layout.fillWidth: true
            Layout.minimumWidth: inputLabelsWidth
            Layout.preferredWidth: inputLabelsWidth
            Layout.maximumWidth: inputLabelsWidth
            text: qsTr("Recovery Key Line 2:")
        }

        CustomTextInput {
            id: keyLine2
            font: fixedFont
            Layout.fillWidth: true
            validator: EasyEncValidator {
                id: line2Validator
                name: qsTr("Line 2")
            }
            selectByMouse: true
            onAcceptableInputChanged: {
                if (acceptableLines && !keyLine1.acceptableInput) {
                    keyLine1.forceActiveFocus();
                }
            }
            onEditingFinished: {
                if (acceptableLines && keyLine1.acceptableInput) {
                    entryComplete()
                }
            }
        }
    }

    Rectangle {
        color: "transparent"
        height: 25

        Layout.fillWidth: true
        Layout.leftMargin: inputLabelsWidth + 15
        Layout.rightMargin: 15

        CustomLabel {
            id: lblResult
            visible: acceptableLines && privateRootKeyToCheck.length
            topPadding: 1
            bottomPadding: 1
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: keyLine1.width / 2
            height: 22
            text: privateRootKey === privateRootKeyToCheck ? qsTr("Key is correct") : qsTr("Wrong key")
            color: privateRootKey === privateRootKeyToCheck ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
        }

        CustomLabel {
            id: lblResultLine1
            width: keyLine1.width / 2
            height: 22
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            visible: !acceptableLines || !privateRootKeyToCheck.length
            topPadding: 1
            bottomPadding: 1
            text: keyLine1.validator.statusMsg
            color: keyLine1.acceptableInput ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
        }
        CustomLabel {
            width: keyLine1.width / 2
            height: 22
            id: lblResultLine2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            horizontalAlignment: Text.AlignRight

            visible: !acceptableLines || !privateRootKeyToCheck.length
            topPadding: 1
            bottomPadding: 1
            text: keyLinesIdentical ? identicalLinesErrorMsg : keyLine2.validator.statusMsg
            color: keyLine1.text === keyLine2.text || !keyLine2.acceptableInput ?
                       BSStyle.inputsInvalidColor : BSStyle.inputsValidColor
        }
    }
}
