import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.PasswordConfirmValidator 1.0

import "../StyledControls"
import "../BsStyles"

ColumnLayout {
    id: topLayout

    property int labelsWidth: 110
    property int inputsWidth: 250
    property int rowSpacing: 5
    property bool acceptableInput: passwordInput.acceptableInput &&
                                   passwordInput.text.length > 0
    property alias columnSpacing: topLayout.spacing
    property alias password: passwordInput.text
    property alias passwordLabelTxt: passwordLabel.text
    property alias passwordInputPlaceholder: passwordInput.placeholderText
    property alias confirmLabelTxt: confirmPasswordLabel.text
    property alias confirmInputPlaceholder: passwordInput.placeholderText


    RowLayout {
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.alignment: Qt.AlignTop

        CustomLabel {
            id: passwordLabel
            elide: Label.ElideRight
            text: qsTr("Password:")
            wrapMode: Text.WordWrap
            Layout.minimumWidth: labelsWidth
            Layout.preferredWidth: labelsWidth
            Layout.maximumWidth: labelsWidth
            Layout.fillWidth: true
        }

        CustomPasswordTextInput {
            id: passwordInput
            focus: true
            maximumLength: 32
            Layout.fillWidth: true
            implicitWidth: inputsWidth
            validator: PasswordConfirmValidator {
                compareTo: confirmPasswordInput.text
            }
        }
    }

    RowLayout {
        id: row1
        spacing: rowSpacing
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.alignment: Qt.AlignTop

        CustomLabel {
            id: confirmPasswordLabel
            elide: Label.ElideRight
            text: qsTr("Confirm Password:")
            wrapMode: Text.WordWrap
            Layout.minimumWidth: labelsWidth
            Layout.preferredWidth: labelsWidth
            Layout.maximumWidth: labelsWidth
            Layout.fillWidth: true
        }

        CustomPasswordTextInput {
            id: confirmPasswordInput
            focus: true
            maximumLength: 32
            Layout.fillWidth: true
            implicitWidth: inputsWidth
            validator: PasswordConfirmValidator {}
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignTop
        opacity: passwordInput.validator.statusMsg === "" ? 0.0 : 1.0
        spacing: 5
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.preferredWidth: inputsWidth + labelsWidth


        CustomLabel {
            topPadding: 1
            bottomPadding: 1
            Layout.fillWidth: true
            Layout.leftMargin: labelsWidth + 5
            text: passwordInput.validator.statusMsg
            color: passwordInput.acceptableInput ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor;
        }
    }
}
