import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0

ColumnLayout {
    id: topLayout

    property int inputLablesWidth: 110
    property int rowSpacing: 5
    property bool acceptableInput: confirmPasswordInput.acceptableInput &&
                                   confirmPasswordInput.text.length
    property alias columnSpacing: topLayout.spacing
    property alias text: confirmPasswordInput.text
    property alias passwordLabelTxt: passwordLabel.text
    property alias passwordInputPlaceholder: passwordInput.placeholderText
    property alias confirmLabelTxt: confirmPasswordLabel.text
    property alias confirmInputPlaceholder: confirmPasswordInput.placeholderText


    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.alignment: Qt.AlignTop

        BSInputLabel {
            id: passwordLabel
            elide: Label.ElideRight
            text: qsTr("Password:")
            wrapMode: Text.WordWrap
            Layout.minimumWidth: inputLablesWidth
            Layout.preferredWidth: inputLablesWidth
            Layout.maximumWidth: inputLablesWidth
            Layout.fillWidth: true
        }

        BSTextInput {
            id: passwordInput
            focus: true
            echoMode: TextField.Password
            Layout.fillWidth: true
        }
    }

    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.alignment: Qt.AlignTop

        BSInputLabel {
            id: confirmPasswordLabel
            elide: Label.ElideRight
            text: qsTr("Confirm Password:")
            wrapMode: Text.WordWrap
            Layout.minimumWidth: inputLablesWidth
            Layout.preferredWidth: inputLablesWidth
            Layout.maximumWidth: inputLablesWidth
            Layout.fillWidth: true
        }

        BSTextInput {
            id: confirmPasswordInput
            focus: true
            echoMode: TextField.Password
            Layout.fillWidth: true
            validator: PasswordConfirmValidator {
                compareTo: passwordInput.text
            }
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignTop
        opacity: confirmPasswordInput.validator.statusMsg === "" ? 0.0 : 1.0
        spacing: 5
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        BSInputLabel {
            topPadding: 1
            bottomPadding: 1
            Layout.fillWidth: true
            Layout.leftMargin: inputLablesWidth + 5
            text:  confirmPasswordInput.validator.statusMsg
            color: confirmPasswordInput.acceptableInput ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor;
        }
    }
}
