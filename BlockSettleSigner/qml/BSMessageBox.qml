import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.0

CustomDialog {
    id: root
    width: 350
    height: 210
    property alias titleText: labelTitle.text
    property alias text: labelText.text
    property alias details: labelDetails.text
    property alias acceptButtonText: acceptButton.text
    property alias rejectButtonText: rejectButton.text
    property alias rejectButtonVisible: rejectButton.visible
    property int buttonWidth: 110
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

    FocusScope {
        id: focus
        anchors.fill: parent
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

        CustomHeaderPanel {
            id: labelTitle
            height: 40
            width: parent.width
            anchors.top: parent.top
            text: titleText
        }

        RowLayout {
            anchors.top: labelTitle.bottom
            anchors.bottom: buttonBar.top
            width: parent.width

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
                    }

                    CustomLabel{
                        id: labelDetails
                        Layout.preferredWidth: textRect.width
                    }
                }

            }
        }
        CustomButtonBar {
            id: buttonBar
            anchors.bottom: parent.bottom
            implicitHeight: 45

            CustomButton {
                id: rejectButton
                width: buttonWidth
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 5
                visible: true
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
