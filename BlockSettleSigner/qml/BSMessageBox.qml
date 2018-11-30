import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.0

import "bscontrols"

CustomDialog {
    id: root
    width: 350
    property alias titleText: labelTitle.text
    property alias text: labelText.text
    property alias details: labelDetails.text
    property alias acceptButtonText: acceptButton.text
    property alias rejectButtonText: rejectButton.text
    property alias rejectButtonVisible: rejectButton.visible
    property int buttonWidth: 110
    property bool usePassword: false
    // this allows the messagebox to grow dynamically with large amount of text
    //value of 20 is margins in textRect
    property int newHeight: labelTitle.height + labelText.height + labelDetails.height + buttonBar.height + 20
    property int type: BSMessageBox.Type.Info
    property variant images: ["/NOTIFICATION_INFO", "/NOTIFICATION_SUCCESS", "/NOTIFICATION_QUESTION", "/NOTIFICATION_WARNING", "/NOTIFICATION_CRITICAL"]
    property variant colors: ["#ffffff", "#38C673", "#f7b03a", "#f7b03a", "#EE2249"]

    enum Type {
       Info = 0,
       Success = 1,
       Question = 2,
       Warning = 3,
       Critical = 4
    }

    contentItem: FocusScope {
        id: focus
        anchors.fill: parent
        anchors.margins: 0
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                accept();
            } else if (event.key === Qt.Key_Escape) {
                if (rejectButton.visible) {
                    reject();
                }
            }
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0

            CustomHeaderPanel {
                id: labelTitle
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                text: titleText
            }
            RowLayout {
                Image {
                    Layout.margins: 10
                    Layout.rightMargin: 0
                    Layout.alignment: Qt.AlignTop
                    source: images[type]
                }

                Item {
                    id: textRect
                    Layout.margins: 10
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    Layout.preferredHeight: childrenRect.height

                    ColumnLayout {
                        CustomLabelValue{
                            id: labelText
                            padding: 0
                            Layout.preferredWidth: textRect.width
                            color: colors[type]
                            Layout.minimumHeight: 10
                        }

                        CustomLabel{
                            id: labelDetails
                            Layout.preferredWidth: textRect.width
                            Layout.minimumHeight: 60
                        }
                    }
                }
            }
            RowLayout {
                CustomLabel {
                    visible: usePassword
                    Layout.fillWidth: true
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.preferredWidth: 105
                    text:   qsTr("Password")
                }
                BSTextInput {
                    id: passwordInput
                    visible: usePassword
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    focus: true
                    echoMode: TextField.Password
                    //placeholderText: qsTr("Password")
                    Layout.fillWidth: true
                }
            }
            CustomButtonBar {
                id: buttonBar
                Layout.fillWidth: true
                Layout.preferredHeight: 45

                CustomButton {
                    id: rejectButton
                    width: buttonWidth
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    visible: type === BSMessageBox.Type.Question
                    text: qsTr("Cancel")
                    onClicked: {
                        reject()
                    }
                }
                CustomButtonPrimary {
                    id: acceptButton
                    width: buttonWidth
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    text: qsTr("OK")
                    enabled: true
                    onClicked: {
                        accept()
                    }
                }
            }
        }
    }
}
