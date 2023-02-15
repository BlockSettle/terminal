
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

Image {
    id: control

    signal deleteRequested()

    width: 24
    height: 24
    sourceSize.width: 24
    sourceSize.height: 24

    anchors.verticalCenter: parent.verticalCenter
    source: "qrc:/images/delete.png"
    MouseArea {
        anchors.fill: parent

        onClicked: {
            control.deleteRequested()
        }
    }
}
