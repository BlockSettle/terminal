//import QtQuick 2.9
//import QtQuick.Controls 2.4
//import QtQuick.Controls.Styles 1.4
//import QtQuick.Layouts 1.0

//import "../BsStyles"

//CustomTitleDialogWindow {
//    id: root

//    property bool acceptButtonVisible: true
//    property bool cancelButtonVisible: true

//    property string customText
//    property string customDetails
//    property variant images: ["/NOTIFICATION_INFO", "/NOTIFICATION_SUCCESS", "/NOTIFICATION_QUESTION", "/NOTIFICATION_WARNING", "/NOTIFICATION_CRITICAL"]
//    property variant colors: [BSStyle.dialogTitleWhiteColor, BSStyle.dialogTitleGreenColor, BSStyle.dialogTitleOrangeColor, BSStyle.dialogTitleOrangeColor,  BSStyle.dialogTitleRedColor]
//    property int messageType: CustomMessageDialog.MessageType.Info
//    property alias acceptButton: acceptButton_
//    property alias rejectButton: rejectButton_

//    property alias labelText: labelText_
//    property alias labelDetails: labelDetails_

//    default property alias messageDialogContentItem: container.data

//    enum MessageType {
//       Info = 0,
//       Success = 1,
//       Question = 2,
//       Warning = 3,
//       Critical = 4
//    }
//    width: 350
//    acceptable: true
//    rejectable: true

//    cContentItem: ColumnLayout {
//        Layout.fillWidth: true

//        RowLayout {
//            id: rowLayout_0
//            Layout.fillWidth: true
//            spacing: 5
//            Layout.preferredWidth: root.width

//            Image {
//                id: image
//                Layout.margins: 10
//                Layout.rightMargin: 0
//                Layout.alignment: Qt.AlignTop
//                source: images[messageType]
//            }

//            ColumnLayout {
//                Layout.preferredWidth: root.width

//                CustomLabelValue{
//                    id: labelText_
//                    padding: 15
//                    Layout.preferredWidth: root.width - image.width - 10
//                    color: colors[messageType]
//                    Layout.minimumHeight: 40
//                    text: customText
//                }

//                CustomLabel{
//                    id: labelDetails_
//                    text: customDetails
//                    padding: 15
//                    //textFormat: Text.RichText
//                    Layout.preferredWidth: root.width - image.width - 10
//                    onLinkActivated: Qt.openUrlExternally(link)
//                    MouseArea {
//                        anchors.fill: parent
//                        acceptedButtons: Qt.NoButton
//                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
//                    }
//                }
//            }

//        }

//        RowLayout {
//            id: container
//        }
//    }

//    cFooterItem: RowLayout {
//        Layout.fillWidth: true
//        CustomButtonBar {
//            id: buttonBar
//            Layout.fillWidth: true
//            Layout.preferredHeight: 45

//            CustomButton {
//                id: rejectButton_
//                anchors.left: acceptButtonVisible && cancelButtonVisible ? parent.left : undefined
//                anchors.horizontalCenter: acceptButtonVisible && cancelButtonVisible ? undefined : parent.horizontalCenter
//                anchors.bottom: parent.bottom
//                anchors.margins: 5
//                visible: cancelButtonVisible
//                text: qsTr("Cancel")
//                onClicked: {
//                    reject()
//                }
//            }

//            CustomButtonPrimary {
//                id: acceptButton_
//                anchors.right: acceptButtonVisible && cancelButtonVisible ? parent.right: undefined
//                anchors.horizontalCenter: acceptButtonVisible && cancelButtonVisible ? undefined : parent.horizontalCenter
//                anchors.bottom: parent.bottom
//                anchors.margins: 5
//                text: qsTr("OK")
//                visible: acceptButtonVisible
//                onClicked: {
//                    acceptAnimated()
//                }
//            }
//        }

//    }
//}

//// -----
//CustomDialogWindow {
//    id: root
//    width: 350

//    property bool acceptable: true
//    property bool rejectable: true

//    property alias acceptButton: acceptButton_
//    property alias rejectButton: rejectButton_

//    property alias labelTitle : labelTitle_
//    property alias labelText: labelText_
//    property alias labelDetails: labelDetails_


//    default property alias customContentItem: customContentItemContainer.data


//    property string customTitle: qsTr("Notice!")
//    property string customText
//    property string customDetails
//    property variant images: ["/NOTIFICATION_INFO", "/NOTIFICATION_SUCCESS", "/NOTIFICATION_QUESTION", "/NOTIFICATION_WARNING", "/NOTIFICATION_CRITICAL"]
//    property variant colors: [BSStyle.dialogTitleWhiteColor, BSStyle.dialogTitleGreenColor, BSStyle.dialogTitleOrangeColor, BSStyle.dialogTitleOrangeColor,  BSStyle.dialogTitleRedColor]
//    property int type: CustomMessageDialog.Type.Info

//    enum Type {
//       Info = 0,
//       Success = 1,
//       Question = 2,
//       Warning = 3,
//       Critical = 4
//    }

//    Component.onCompleted: customContentItem.parent = customContentItemContainer

//    contentItem: FocusScope {

//        id: focus
//        anchors.fill: parent
//        anchors.margins: 0
//        focus: true

//        Keys.onPressed: {
//            event.accepted = true
//            if (event.modifiers === Qt.ControlModifier)
//                switch (event.key) {
//                case Qt.Key_A:
//                    detailedText.selectAll()
//                    break
//                case Qt.Key_C:
//                    detailedText.copy()
//                    break
//                case Qt.Key_Period:
//                    if (Qt.platform.os === "osx")
//                        if (rejectable) reject()
//                    break
//            } else switch (event.key) {
//                case Qt.Key_Escape:
//                case Qt.Key_Back:
//                    if (rejectable) reject()
//                    break
//                case Qt.Key_Enter:
//                case Qt.Key_Return:
//                    if (acceptable) acceptAnimated()
//                    break
//            }
//        }

//        ColumnLayout {
//            anchors.fill: parent
//            anchors.margins: 0

//            CustomHeaderPanel {
//                id: labelTitle_
//                Layout.preferredHeight: 40
//                Layout.fillWidth: true
//                text: customTitle
//            }

//            RowLayout {
//                id: rowLayout_0
//                Layout.fillWidth: true
//                spacing: 5
//                Layout.preferredWidth: root.width

//                Image {
//                    id: image
//                    Layout.margins: 10
//                    Layout.rightMargin: 0
//                    Layout.alignment: Qt.AlignTop
//                    source: images[type]
//                }

//                ColumnLayout {
//                    Layout.preferredWidth: root.width

//                    CustomLabelValue{
//                        id: labelText_
//                        padding: 15
//                        Layout.preferredWidth: root.width - image.width - 10
//                        color: colors[type]
//                        Layout.minimumHeight: 40
//                        text: customText
//                    }

//                    CustomLabel{
//                        id: labelDetails_
//                        text: customDetails
//                        padding: 15
//                        //textFormat: Text.RichText
//                        Layout.preferredWidth: root.width - image.width - 10
//                        onLinkActivated: Qt.openUrlExternally(link)
//                        MouseArea {
//                            anchors.fill: parent
//                            acceptedButtons: Qt.NoButton
//                            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
//                        }
//                    }
//                }
//            }

//            RowLayout {
//                id: customContentItemContainer
//            }

//            CustomButtonBar {
//                id: buttonBar
//                Layout.fillWidth: true
//                Layout.preferredHeight: 45

//                CustomButton {
//                    id: rejectButton_
//                    anchors.left: parent.left
//                    anchors.bottom: parent.bottom
//                    anchors.margins: 5
//                    visible: rejectable
//                    text: qsTr("Cancel")
//                    onClicked: {
//                        reject()
//                    }
//                }

//                CustomButtonPrimary {
//                    id: acceptButton_
//                    anchors.right: parent.right
//                    anchors.bottom: parent.bottom
//                    anchors.margins: 5
//                    text: qsTr("OK")
//                    visible: acceptable
//                    onClicked: {
//                        acceptAnimated()
//                    }
//                }
//            }
//        }
//    }
//}
