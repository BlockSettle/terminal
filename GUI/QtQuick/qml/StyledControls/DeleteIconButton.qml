
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

Image {
    id: control

    signal deleteRequested()

    width: BSSizes.applyScale(24)
    height: BSSizes.applyScale(24)
    sourceSize.width: BSSizes.applyScale(24)
    sourceSize.height: BSSizes.applyScale(24)

    anchors.verticalCenter: parent.verticalCenter
    source: "qrc:/images/delete.png"
    MouseArea {
        anchors.fill: parent

        onClicked: {
            control.deleteRequested()
        }
    }
}
