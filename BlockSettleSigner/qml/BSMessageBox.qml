import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.0

CustomDialog {
    width: 350
    height: 200
    property alias titleText: labelTitle.text
    property alias text: labelText.text
    property alias details: labelDetails.text

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        width: parent.width
        id: mainLayout

        RowLayout{
            CustomHeaderPanel{
                Layout.preferredHeight: 40
                id: labelTitle
                Layout.fillWidth: true
                text: titleText

            }
        }
        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabelValue{
                id: labelText
                Layout.fillWidth: true
                text: text
            }
        }
        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabelValue{
                id: labelDetails
                Layout.fillWidth: true
            }
        }

        CustomButtonBar {
            implicitHeight: childrenRect.height
            Layout.fillWidth: true


            RowLayout {
                anchors.fill: parent

                CustomButton {
                    visible: true
                    Layout.fillWidth: true
                    Layout.margins: 5
                    text: qsTr("Cancel")
                    onClicked: {
                        reject()
                    }
                }

                CustomButtonPrimary {
                    Layout.fillWidth: true
                    Layout.margins: 5
                    text: qsTr("Continue")
                    enabled: true
                    onClicked: {
                        accept()
                    }
                }

            }
        }
    }
}
