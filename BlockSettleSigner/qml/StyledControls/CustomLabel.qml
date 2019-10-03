import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Label {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: { enabled ? BSStyle.labelsTextColor : BSStyle.disabledColor }
    wrapMode: Text.WordWrap
    topPadding: 5
    bottomPadding: 5
}

