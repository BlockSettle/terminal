import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Label {
    font.pixelSize: 11
    color: "white"
    wrapMode: Text.WordWrap
    topPadding: 5
    bottomPadding: 5
    onLinkActivated: Qt.openUrlExternally(link)
}
