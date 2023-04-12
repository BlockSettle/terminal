import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"

Window {
    id: root

    property int navig_bar_width: BSSizes.applyScale(36)
    property alias _stack_view: stack_popup
    property alias _arrow_but_visibility: back_arrow_button.visible

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    maximumHeight: rect.height
    maximumWidth: rect.width

    minimumHeight: rect.height
    minimumWidth: rect.width

    height: rect.height
    width: rect.width

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width) / 2
    y: mainWindow.y + (mainWindow.height - height) / 2

    signal sig_close_click()
    signal sig_back_arrow_click()

    Rectangle {
        id: rect

        color: BSStyle.popupBackgroundColor
        opacity: 1
        radius: BSSizes.applyScale(16)
        height: stack_popup.height + navig_bar_width
        width: stack_popup.width
        border.color : BSStyle.defaultBorderColor
        border.width : BSSizes.applyScale(1)


        Rectangle {
            id: close_rect

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(1)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(1)
            radius: BSSizes.applyScale(16)

            color: BSStyle.popupBackgroundColor
            height: BSSizes.applyScale(39)
            width: BSSizes.applyScale(110)

            Image {
                id: close_button

                anchors.top: parent.top
                anchors.topMargin: BSSizes.applyScale(23)
                anchors.right: parent.right
                anchors.rightMargin: BSSizes.applyScale(23)

                source: "qrc:/images/close_button.svg"
                width: BSSizes.applyScale(16)
                height: BSSizes.applyScale(16)
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    sig_close_click()
                }
            }
        }

        Image {
            id: back_arrow_button

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(24)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(24)

            source: "qrc:/images/back_arrow.png"
            width: BSSizes.applyScale(20)
            height: BSSizes.applyScale(16)
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    sig_back_arrow_click()
                }
            }
        }

        StackView {
            id: stack_popup

            anchors.top: close_rect.bottom
            anchors.topMargin: 0

            implicitHeight: currentItem.height
            implicitWidth: currentItem.width


            pushEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 200
                }
            }

            pushExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 200
                }
            }

            popEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 200
                }
            }

            popExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 200
                }
            }

            replaceEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 10
                }
            }

            replaceExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 10
                }
            }
        }

    }

    onSig_close_click: {
        close_click()
    }

    onSig_back_arrow_click: {
        stack_popup.pop()
    }

    function close_click()
    {
        root.close()
        stack_popup.pop(null)
    }

}
