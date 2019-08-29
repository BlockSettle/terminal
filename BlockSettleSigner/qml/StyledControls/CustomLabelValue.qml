import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Label {
    font.pixelSize: 12
    color: "white"
    wrapMode: Text.WordWrap
    padding: 5
    onLinkActivated: Qt.openUrlExternally(link)
}
