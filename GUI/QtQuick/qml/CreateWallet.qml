import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "BsStyles"

Window {
    id: root
    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog

    maximumHeight: rect.height
    maximumWidth: rect.width

    minimumHeight: rect.height
    minimumWidth: rect.width

    objectName: "create_wallet"

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + 28

    Rectangle {
        id: rect
        color: "#191E2A"
        opacity: 1
        radius: 16
        height: stack_create_wallet.height + 40
        width: stack_create_wallet.width
        //anchors.fill: parent
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

        StackView {
            id: stack_create_wallet
            initialItem: terms_conditions

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
        }

        TermsAndConditions {
            id: terms_conditions
            visible: false
            onSig_continue: {
                stack_create_wallet.push(start_create)
            }
        }

        StartCreateWallet {
            id: start_create
            visible: false
        }
    }
}

