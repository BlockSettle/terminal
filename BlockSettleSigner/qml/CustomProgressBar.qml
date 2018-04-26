import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
ProgressBar {
    id: control
    value: 0.5
    padding: 1

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 6
        color: "#000000"
        radius: 3
    }

    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 4

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: "#22C064"
        }
    }
}

