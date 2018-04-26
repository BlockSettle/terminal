import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
Dialog {
    x: (width > parent.width) ? 0 : (parent.width - width) / 2
    y: (height > parent.height) ? 0 : (parent.height - height) / 2

    focus: true
    modal: true
    padding: 0
    background: Rectangle {
        color:  "#1C2835"
        border.color:  "#0A1619"
    }
}

