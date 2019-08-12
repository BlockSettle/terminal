import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../BsStyles"
import "../StyledControls"

CustomTitleDialogWindow {
    id: root

    property bool acceptButtonVisible: true
    property bool cancelButtonVisible: true

    property string customText
    property string customDetails
    property variant images: ["/NOTIFICATION_INFO", "/NOTIFICATION_SUCCESS", "/NOTIFICATION_QUESTION", "/NOTIFICATION_WARNING", "/NOTIFICATION_CRITICAL"]
    property variant colors: [BSStyle.dialogTitleWhiteColor, BSStyle.dialogTitleGreenColor, BSStyle.dialogTitleOrangeColor, BSStyle.dialogTitleOrangeColor,  BSStyle.dialogTitleRedColor]
    property int type: BSMessageBox.Type.Info
    property alias acceptButton: acceptButton_
    property alias rejectButton: rejectButton_

    property alias labelText: labelText_
    property alias labelDetails: labelDetails_

    default property alias messageDialogContentItem: container.data

    enum Type {
       Info = 0,
       Success = 1,
       Question = 2,
       Warning = 3,
       Critical = 4
    }
    width: 350
    //height: 70 + headerPanelHeight + contentItemData.height
    acceptable: true
    rejectable: true

    cContentItem: ColumnLayout {
        id: contentItemData
        Layout.fillWidth: true

        RowLayout {
            id: rowLayout_0
            Layout.fillWidth: true
            spacing: 5

            Image {
                id: image
                Layout.margins: 10
                Layout.rightMargin: 0
                Layout.alignment: Qt.AlignTop
                source: images[type]
            }

            ColumnLayout {
                Layout.preferredWidth: root.width

                CustomLabelValue{
                    id: labelText_
                    topPadding: 25
                    leftPadding: 15
                    rightPadding: 6
                    Layout.preferredWidth: root.width - image.width - leftPadding - rightPadding
                    //color: colors[type]
                    color: BSStyle.dialogTitleWhiteColor
                    text: customText
                }

                CustomLabel{
                    id: labelDetails_
                    text: customDetails
                    leftPadding: 15
                    rightPadding: 6
                    //textFormat: Text.RichText
                    Layout.preferredWidth: root.width - image.width - leftPadding - rightPadding
                    onLinkActivated: Qt.openUrlExternally(link)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }
        }

        RowLayout {
            id: container
        }
    }

    cFooterItem: RowLayout {
        Layout.fillWidth: true
        CustomButtonBar {
            id: buttonBar
            Layout.fillWidth: true
            Layout.preferredHeight: 45

            CustomButton {
                id: rejectButton_
                anchors.left: acceptButtonVisible && cancelButtonVisible ? parent.left : undefined
                anchors.horizontalCenter: acceptButtonVisible && cancelButtonVisible ? undefined : parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.margins: 5
                visible: cancelButtonVisible
                text: qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                id: acceptButton_
                anchors.right: acceptButtonVisible && cancelButtonVisible ? parent.right: undefined
                anchors.horizontalCenter: acceptButtonVisible && cancelButtonVisible ? undefined : parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("OK")
                visible: acceptButtonVisible
                onClicked: {
                    acceptAnimated()
                }
            }
        }

    }
}

