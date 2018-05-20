import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.EasyEncValidator 1.0

ColumnLayout {
    id: topLayout

    Layout.topMargin: 2 * columnSpacing
    Layout.bottomMargin: 2 * columnSpacing

    property bool acceptableInput: (!keyLinesIdentical &&
                                   keyLine1.acceptableInput &&
                                   keyLine2.acceptableInput)

    property alias line1LabelTxt: keyLine1Label.text
    property alias line2LabelTxt: keyLine2Label.text
    property alias sectionHeaderTxt: sectionHeader.text

    property int inputLablesWidth: 110
    property int rowSpacing: 5
    property int columnSpacing: topLayout.spacing

    property bool keyLinesIdentical: keyLine1.text.length &&
                                     keyLine1.text === keyLine2.text &&
                                     keyLine1.acceptableInput

    property string easyCodePalceholder1: "nnnn nnnn nnnn nnnn nnnn nnnn nnnn nnnn nnnn"
    property string easyCodePalceholder2: "kkkk kkkk kkkk kkkk kkkk kkkk kkkk kkkk kkkk"
    property string identicalLinesErrorMsg: "Same Code Used in Line 1 and Line 2"

    property string  privateRootKey: keyLine1.text + " " + keyLine2.text


    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        BSInputLabel {
            id: sectionHeader
            Layout.leftMargin: inputLablesWidth
            Layout.fillWidth: true
            Layout.minimumWidth: inputLablesWidth
            Layout.preferredWidth: inputLablesWidth
            Layout.maximumWidth: inputLablesWidth
            text: qsTr("Enter Recovery Code: ")
        }
    }

    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        BSInputLabel {
            id: keyLine1Label
            Layout.fillWidth: true
            Layout.minimumWidth: inputLablesWidth
            Layout.preferredWidth: inputLablesWidth
            Layout.maximumWidth: inputLablesWidth
            text: qsTr("Recovery key Line 1:")
        }

        BSTextInput {
            id: keyLine1
            Layout.fillWidth: true
            selectByMouse: true
            activeFocusOnPress: true
            validator: EasyEncValidator {
                            id: line1Validator;
                            name: qsTr("Line 1")
                        }
            onAcceptableInputChanged: {
                if (acceptableInput && !keyLine2.acceptableInput) {
                    keyLine2.forceActiveFocus();
                }
            }
        }
    }

    RowLayout {
        opacity: keyLine1.validator.statusMsg === "" ? 0.0 : 1.0
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        BSInputLabel {
            topPadding: 1
            bottomPadding: 1
            Layout.fillWidth: true
            Layout.leftMargin: inputLablesWidth + 5
            text:  keyLine1.validator.statusMsg
            color: keyLine1.acceptableInput ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
        }
    }

    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        BSInputLabel {
            id: keyLine2Label
            Layout.fillWidth: true
            Layout.minimumWidth: inputLablesWidth
            Layout.preferredWidth: inputLablesWidth
            Layout.maximumWidth: inputLablesWidth
            text: qsTr("Recovery Key Line 2:")
        }

        BSTextInput {
            id: keyLine2
            Layout.fillWidth: true
            validator: EasyEncValidator { id: line2Validator; name: qsTr("Line 2") }
            selectByMouse: true
            activeFocusOnPress: true
            onAcceptableInputChanged: {
                if (acceptableInput && !keyLine1.acceptableInput) {
                    keyLine1.forceActiveFocus();
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        BSInputLabel {
            opacity: keyLine2.validator.statusMsg === "" ? 0.0 : 1.0
            topPadding: 1
            bottomPadding: 1
            Layout.fillWidth: true
            Layout.leftMargin: inputLablesWidth + 5
            text:  keyLinesIdentical ? identicalLinesErrorMsg : keyLine2.validator.statusMsg
            color: keyLine1.text === keyLine2.text || !keyLine2.acceptableInput ?
                       BSStyle.inputsInvalidColor : BSStyle.inputsValidColor
        }
    }

}
