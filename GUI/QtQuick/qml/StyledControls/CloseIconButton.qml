
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

Image {
    id: control

    signal close();

    width: BSSizes.applyScale(16)
    height: BSSizes.applyScale(16)
    source: "qrc:/images/close_button.svg"

    MouseArea {
        anchors.fill: parent
        onClicked: {
            control.close()
        }
    }
}
