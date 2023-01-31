import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15


Window {
    id: root

    property int navig_bar_width: 36
    property alias _stack_view: stack_create_wallet
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

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + 28

    Rectangle {
        id: rect

        property var phrase
        color: "#191E2A"
        opacity: 1
        radius: 16
        height: stack_create_wallet.height + navig_bar_width
        width: stack_create_wallet.width
        border.color : "#3C435A"
        border.width : 1

        Image {
            id: close_button

            anchors.top: parent.top
            anchors.topMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24

            source: "qrc:/images/close_button.png"
            width: 16
            height: 16
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   root.close()
                   stack_create_wallet.pop(null)
                }
            }
        }

        Image {
            id: back_arrow_button

            anchors.top: parent.top
            anchors.topMargin: 24
            anchors.left: parent.left
            anchors.leftMargin: 24

            source: "qrc:/images/back_arrow.png"
            width: 20
            height: 16
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   stack_create_wallet.pop()
                }
            }
        }

        StackView {
            id: stack_create_wallet

            anchors.top: close_button.bottom
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

}
